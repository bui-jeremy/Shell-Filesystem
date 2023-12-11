#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>

#define DEVICE_PATH "/proc/ramdisk"
#define MY_IOCTL_CREATE_RAMDISK _IO('k', 1) // Custom ioctl command

int main() {
    int fd;

    // Open the device file
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device.");
        return EXIT_FAILURE;
    }

    // Trigger the ioctl command to create the RAM disk
    if (ioctl(fd, MY_IOCTL_CREATE_RAMDISK) < 0) {
        perror("ioctl failed.");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("RAM disk created through ioctl.\n");

    // Close the device file
    close(fd);

    return EXIT_SUCCESS;
}
