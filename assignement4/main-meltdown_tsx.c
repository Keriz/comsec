#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

/* for ioctl */
#define WOM_MAGIC_NUM 0x1337
#define WOM_GET_ADDRESS \
    _IOR(WOM_MAGIC_NUM, 0, unsigned long)

void *
wom_get_address(int fd)
{
    void *addr = NULL;

    if (ioctl(fd, WOM_GET_ADDRESS, &addr) < 0)
        return NULL;

    return addr;
}

int main(int argc, char *argv[])
{
    const char *secret;
    int fd;

    fd = open("/dev/wom", O_RDONLY);

    if (fd < 0) {
        perror("open");
        fprintf(stderr, "error: unable to open /dev/wom. "
            "Please build and load the wom kernel module.\n");
        return -1;
    }

    secret = wom_get_address(fd);

    // Your code goes here instead of the following 2 lines
    printf("secret=%p\n", secret);
    printf("742527b55fa326108d952fa713239ae5\n");

    close(fd);

    return 0;

err_close:
    close(fd);
    return -1;
}
