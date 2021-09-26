#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SUPERPAGE (1 << 30)
#define PAGE_SIZE (1 << 16)

#define POOL_SIZE 5000
#define ROUNDS 30

#define SUBSET_SIZE 2000
#define MAX_NUMBER_BANKS 32
#define MIN_BANK_SUBSET_SIZE 80

static char *pool[POOL_SIZE];
static char *poolSubset[SUBSET_SIZE];

static char *buffer;

int compare(const void *t1, const void *t2) {
	return (int)(*(uint64_t *)t1 - *(uint64_t *)t2);
}

uint64_t median(uint64_t times[], size_t size) {
	qsort(times, size, sizeof(uint64_t), compare);
	uint16_t n = ((size + 1) / 2) - 1;
	return times[n];
}

uint64_t
time_access(volatile char *addr1, volatile char *addr2) {
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

		asm volatile("lfence\n\t");
		asm volatile("RDTSCP\n\t"
		             "mov %%edx, %0\n\t"
		             "mov %%eax, %1\n\t"
		             "CPUID\n\t"
		             : "=r"(cycles_high1), "=r"(cycles_low1)::"%rax", "%rbx", "%rcx", "%rdx");

		asm volatile("clflushopt (%0)\n\t"
		             "clflushopt (%1)\n\t" ::"r"(addr1),
		             "r"(addr2)
		             : "memory");

		t0 = (((uint64_t)cycles_high << 32) | (uint64_t)cycles_low);
		t1 = (((uint64_t)cycles_high1 << 32) | (uint64_t)cycles_low1);

		//times[0] will never have a value
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

//credits to the Bit Twiddling Hacks (http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2)
uint16_t round_power_2(uint16_t v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v++;
	return v;
}

int main(int argc, char **argv) {

	time_t t;

	srand((unsigned)time(&t));

	buffer = mmap(NULL, SUPERPAGE, PROT_READ | PROT_WRITE,
	              MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

	randomize_pool();

	char thresholdFlag        = 0,
	     autoThresholdFlag    = 0,
	     printCSVFlag         = 0,
	     thresholdNotComputed = 1;

	int c;
	uint64_t thresholdValue = 350;
	opterr                  = 0;

	uint16_t poolSubsetSize = 0, totalAddrRemoved = 0, nbBanks = 0;
	uint64_t thresholdTimes[POOL_SIZE] = {0};

	if (argc == 1)
		printCSVFlag = 1;

	while ((c = getopt(argc, argv, "bt:")) != -1) {
		switch (c) {
			case 'b':
				autoThresholdFlag = 1;
				break;
			case 't':
				thresholdFlag  = 1;
				thresholdValue = atoi(optarg);
				break;
		}
	}

	while (totalAddrRemoved < (POOL_SIZE - MIN_BANK_SUBSET_SIZE) && nbBanks < MAX_NUMBER_BANKS) {
		poolSubsetSize               = 0;
		char *base                   = pool[rand() % POOL_SIZE];
		poolSubset[poolSubsetSize++] = base;

		if (printCSVFlag) {
			for (int i = 0; i < POOL_SIZE; ++i) {
				if (pool[i]) {
					uint64_t accessTime = time_access(base, pool[i]);
					printf("%p, %p, %ju\n", base, pool[i], accessTime);
					if (accessTime > thresholdValue) poolSubset[poolSubsetSize++] = pool[i];
				}
			}
		} else {
			if (autoThresholdFlag && thresholdNotComputed) {
				for (int i = 0; i < POOL_SIZE; ++i) {
					thresholdTimes[i] = time_access(base, pool[i]);
				}
				uint64_t medianValue = median(thresholdTimes, POOL_SIZE);
				//thresholdTimes is now sorted from min to max
				thresholdValue = (thresholdTimes[POOL_SIZE - 1] - medianValue) / 2 + medianValue;

				thresholdNotComputed = 0;
			}

			for (int i = 0; i < POOL_SIZE; ++i)
				if ((pool[i]) && time_access(base, pool[i]) > thresholdValue) poolSubset[poolSubsetSize++] = pool[i];
		}

		//in rare cases, we picked a base address belonging to a previously detected bank and thus cannot assign it any bank now as its not matching any leftover addresses
		if ((poolSubsetSize > MIN_BANK_SUBSET_SIZE)) {
			totalAddrRemoved += poolSubsetSize;
			nbBanks++;
			for (int i = 0; i < poolSubsetSize; ++i) {
				for (int j = 0; j < POOL_SIZE; j++)
					if (poolSubset[i] == pool[i])
						pool[i] = NULL;
			}
		}
	}
	if (thresholdFlag)
		printf("%u\n", round_power_2(nbBanks));
	if (autoThresholdFlag)
		printf("%ju %u\n", thresholdValue, round_power_2(nbBanks));
}