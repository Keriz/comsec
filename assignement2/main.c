#include <ctype.h>
#include <stdint.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SUPERPAGE (1 << 30)
#define SUPERPAGE_SIZE 30
#define PAGE_SIZE (1 << 16)

#define POOL_SIZE 5000
#define ROUNDS 50

#define SUBSET_SIZE 2000
#define MAX_NUMBER_BANKS 32
#define MIN_BANK_SUBSET_SIZE 80

static char *pool[POOL_SIZE]                           = {0};
static char *poolSubset[SUBSET_SIZE][MAX_NUMBER_BANKS] = {0};
static uint32_t bankFunctions[MAX_NUMBER_BANKS]        = {0};
static uint8_t bankFunctionsValues[MAX_NUMBER_BANKS]   = {0};

static char *buffer;

int compare(const void *t1, const void *t2) { return (int)(*(uint64_t *)t1 - *(uint64_t *)t2); }

uint64_t median(uint64_t times[], const size_t size) {
	qsort(times, size, sizeof(uint64_t), compare);
	uint16_t n = ((size + 1) / 2) - 1;
	return times[n];
}

uint64_t time_access(const volatile char *addr1, const volatile char *addr2) {
	uint64_t t0, t1;
	uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;

	uint16_t r = ROUNDS;

	static uint64_t times[ROUNDS] = {0};

	while (--r) {

		asm volatile("mfence\n\t");
		asm volatile("CPUID\n\t"
		             "RDTSC\n\t"
		             "mov %%edx, %0\n\t"
		             "mov %%eax, %1\n\t"
		             : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

		asm volatile("mfence\n\t");

		*addr1;
		*addr2;

		asm volatile("mfence\n\t");
		asm volatile("RDTSCP\n\t"
		             "mov %%edx, %0\n\t"
		             "mov %%eax, %1\n\t"
		             "CPUID\n\t"
		             : "=r"(cycles_high1), "=r"(cycles_low1)::"%rax", "%rbx", "%rcx",
		               "%rdx");

		asm volatile("clflushopt (%0)\n\t"
		             "clflushopt (%1)\n\t" ::"r"(addr1),
		             "r"(addr2)
		             : "memory");

		t0 = (((uint64_t)cycles_high << 32) | (uint64_t)cycles_low);
		t1 = (((uint64_t)cycles_high1 << 32) | (uint64_t)cycles_low1);

		// times[0] will never have a value
		times[r] = t1 - t0;
	}

	return median(times, ROUNDS);
}

void randomize_pool() {
	uint64_t addr_offset = SUPERPAGE / POOL_SIZE;
	for (uint32_t i = 0; i < POOL_SIZE; i++) {
		pool[i] = buffer + (i * addr_offset + (rand() % PAGE_SIZE)) % SUPERPAGE;
	}
}

// credits to the Bit Twiddling Hacks
// (http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2)
uint16_t round_power_2(uint16_t v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v++;
	return v;
}

/* inline uint32_t log2(const uint32_t x) {
        return (uint32_t)ceil((ln(x) / ln(2)));
} */
void *change_bit(const char *addr, const char bit) {
	return (void *)((uintptr_t)addr ^ (uintptr_t)(1 << bit));
}

static inline uint8_t self_xor(uint64_t v) { return __builtin_popcountll(v) % 2; }

void get_funcs_values(const char *addr, char *values, const uint8_t nbFunc) {
	for (uint8_t i = 0; i < nbFunc; i++)
		values[i] = self_xor((uint64_t)poolSubset[i][0] & bankFunctions[i]);
}

char *switch_bank(char *addr, const uint8_t bankFuncValues[], const uint8_t nbFunc,
                  uint8_t bitSet) {
	char *newAddr        = addr;
	uint8_t isInDiffBank = 1;

	while (isInDiffBank) {
		for (uint8_t i = 0; i < nbFunc; i++) {
			if (bankFuncValues[i] == self_xor((uint64_t)newAddr & bankFunctions[i])) {
				continue;
				isInDiffBank = 0;
			} else {
				if (__builtin_popcountll(bankFunctions[i] & (1 << bitSet)) > 0) {
					newAddr = (char *)((uint64_t)newAddr ^
					                   (bankFunctions[i] ^ (1 << bitSet)));
				}
				isInDiffBank = 1;
				break;
			}
		}
	}

	return newAddr;
}

int main(int argc, char **argv) {

	time_t t;

	srand((unsigned)time(&t));

	buffer = mmap(NULL, SUPERPAGE, PROT_READ | PROT_WRITE,
	              MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

	randomize_pool();

	char significantBitsFlag = 0, bankAddressingFlag = 0, rowMaskFlag = 0,

	     thresholdNotComputed = 1, nbBankFunctions = 0;

	int c;
	uint64_t thresholdValue = 350;
	opterr                  = 0;

	uint16_t poolSubsetSize = 0, maxPoolSubsetSize = 0, totalAddrRemoved = 0, nbBanks = 0;
	uint32_t significantBits           = 0;
	uint64_t thresholdTimes[POOL_SIZE] = {0};

	while ((c = getopt(argc, argv, "bfm")) != -1) {
		switch (c) {
			case 'b':
				significantBitsFlag = 1;
				break;
			case 'f':
				bankAddressingFlag = 1;
				break;
			case 'm':
				rowMaskFlag = 1;
				break;
		}
	}

	while (totalAddrRemoved < (POOL_SIZE - MIN_BANK_SUBSET_SIZE) &&
	       nbBanks < MAX_NUMBER_BANKS) {
		poolSubsetSize = 0;
		char *base     = pool[rand() % POOL_SIZE];
		while (!base)
			base = pool[rand() % POOL_SIZE];

		poolSubset[poolSubsetSize++][nbBanks] = base;

		if (thresholdNotComputed) {
			for (int i = 0; i < POOL_SIZE; ++i) {
				thresholdTimes[i] = time_access(base, pool[i]);
			}
			uint64_t medianValue = median(thresholdTimes, POOL_SIZE);
			// thresholdTimes is now sorted from min to max
			thresholdValue =
			    (thresholdTimes[POOL_SIZE - 1] - medianValue) / 2 + medianValue;

			thresholdNotComputed = 0;
		}

		for (int i = 0; i < POOL_SIZE; ++i)
			if ((pool[i]) && time_access(base, pool[i]) > thresholdValue)
				poolSubset[poolSubsetSize++][nbBanks] = pool[i];

		// in rare cases, we picked a base address belonging to a
		// previously detected bank and thus cannot assign it any bank
		// now as its not matching any leftover addresses
		if ((poolSubsetSize > MIN_BANK_SUBSET_SIZE)) {
			if (poolSubsetSize > maxPoolSubsetSize) maxPoolSubsetSize = poolSubsetSize;

			totalAddrRemoved += poolSubsetSize;

			for (int i = 0; i < poolSubsetSize; ++i) {
				for (int j = 0; j < POOL_SIZE; j++)
					if (poolSubset[i][nbBanks] == pool[j]) pool[j] = NULL;
			}

			// Recover DRAM addressing bits
			for (int i = 0; i < poolSubsetSize; i++) {
				for (char j = 0; j < SUPERPAGE_SIZE; j++) {
					char *newAddr = change_bit(poolSubset[i][nbBanks], j);
					uint64_t time =
					    time_access(poolSubset[i][nbBanks], newAddr);
					if (time < thresholdValue) {
						significantBits |= (1 << j);
					}
				}
			}

			nbBanks++;
		}
	}

	// bank addressing flags
	uint32_t nbSignificantBits = ceil(log2(significantBits));
	char nbBankBits            = log2(round_power_2(nbBanks));

	uint64_t mask = 0x3;
	while ((uint32_t)ceil(log2(mask)) <= nbSignificantBits) {
		// XORing bits of a variable is eq to counting 1-bits
		// mod 2
		uint64_t fnComp = self_xor((uint64_t)poolSubset[0][0] & mask);

		for (int i = 0; i <= maxPoolSubsetSize; i++) {
			if (!(poolSubset[i][0])) {
				bankFunctions[nbBankFunctions++] = (uint32_t)mask;
				break;
			} else if ((uint64_t)(self_xor((uint64_t)poolSubset[i][0] & mask)) ==
			           fnComp)
				continue;
			else
				break;
		}
		do
			mask += 1;
		// popcount counts the number of 1-bits in x
		while (__builtin_popcountll(mask) != 2);
	}
	if (significantBitsFlag) printf("%08x\n", significantBits);

	if (bankAddressingFlag) {
		printf("%x\n", nbBankFunctions);
		for (int i = 0; i < nbBankFunctions; i++)
			printf("%x\n", bankFunctions[i]);
	}

	if (rowMaskFlag) {
		/* uint32_t rowMaskBits = 0;
		char *a0             = poolSubset[0][0]; // protection agaisnt null values needed
		uint8_t a0_bank[MAX_NUMBER_BANKS] = {0};

		get_funcs_values(a0, a0_bank, nbBankFunctions); */
		/*
		                for (uint8_t bit = 0; bit < SUPERPAGE_SIZE; bit++) {
		                        char *tempA0  = change_bit(a0, bit);
		                        char *newA0   = switch_bank(tempA0, a0_bank,
		   nbBankFunctions, bit); uint64_t time = time_access(a0, newA0); if (time >
		   thresholdValue) { uint32_t highestSetBit = (1 << __builtin_clzll((uint64_t)a0 ^
		   (uint64_t)newA0)); rowMaskBits |= 1 << highestSetBit;
		                        }
		                } */
	}
	fflush(stdout);

	// printf("%d\n", log2(round_power_2(nbBanks)));
}