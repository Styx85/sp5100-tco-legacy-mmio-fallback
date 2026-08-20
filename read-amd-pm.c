#include <stdio.h>
#include <sys/io.h>
#include <unistd.h>

#define PM_INDEX 0xCD6
#define PM_DATA  0xCD7

static unsigned char read_pm(unsigned char reg)
{
    outb(reg, PM_INDEX);
    return inb(PM_DATA);
}

int main(void)
{
    if (geteuid() != 0) {
        fprintf(stderr, "Run as root.\n");
        return 1;
    }

    if (ioperm(PM_INDEX, 2, 1)) {
        perror("ioperm");
        return 1;
    }

    for (unsigned int r = 0x69; r <= 0x6f; r++)
        printf("PM 0x%02x = 0x%02x\n", r, read_pm(r));

    ioperm(PM_INDEX, 2, 0);
    return 0;
}
