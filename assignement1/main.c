#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>

#define SUPERPAGE (1 << 30)
#define POOL_SIZE 100000
#define ROUNDS 5

void swap(uint64_t *p, uint64_t *q) {
    uint64_t t;

    t = *p;
    *p = *q;
    *q = t;
}

void sort(uint64_t a[], char n) {
    uint64_t i, j, temp;

    for (i = 0; i < n - 1; i++) {
	for (j = 0; j < n - i - 1; j++) {
	    if (a[j] > a[j + 1])
		swap(&a[j], &a[j + 1]);
	}
    }
}

uint64_t median(uint64_t times[]) {
    char n = (ROUNDS + 1) / 2 - 1;
    sort(times, ROUNDS);
    return times[n];
}

uint64_t
time_access(char *addr1, char *addr2) {
    uint64_t t0, t1;
    uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;

    char r = ROUNDS;
    uint64_t times[ROUNDS] = {0};

    while ((--r) >= 0) {

	asm volatile("CPUID\n\t"
		     "RDTSC\n\t"
		     "mov %%edx, %0\n\t"
		     "mov %%eax, %1\n\t"
		     : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

	char a = *addr1;
	char b = *addr2;

	asm volatile("RDTSCP\n\t"
		     "mov %%edx, %0\n\t"
		     "mov %%eax, %1\n\t"
		     "CPUID\n\t"
		     : "=r"(cycles_high1), "=r"(cycles_low1)::"%rax", "%rbx", "%rcx", "%rdx");

	asm volatile("mfence\n\t"
		     "clflushopt (%0)\n\t"
		     "clflushopt (%1)\n\t"
		     "mfence\n\t" ::"r"(addr1),
		     "r"(addr2));

	t0 = (((uint64_t)cycles_high << 32) | (uint64_t)cycles_low);
	t1 = (((uint64_t)cycles_high1 << 32) | (uint64_t)cycles_low1);

	times[r] = t1 - t0;
    }

    return median(times);
}

int main(int argc, char *argv) {
    time_t t;

    srand((unsigned)time(&t));

    char *buffer = mmap(NULL, SUPERPAGE, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

    char *base = buffer + rand() % SUPERPAGE;

    for (int i = 0; i < POOL_SIZE; ++i) {
	char *addr = buffer + rand() % SUPERPAGE;
	printf("%p, %p, %ju \n", base, addr, time_access(base, addr));
    }
}