#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <apps/shell.h>
#include <phabos/i2c.h>

static void print_usage(const char *appname)
{
    printf("usage: %s [I2CBUS] [FIRST-ADDR] [LAST-ADDR]\n", appname);
}

static int i2cdetect_main(int argc, char **argv)
{
    int bus = 0;
    unsigned start_addr = 0x3;
    unsigned end_addr = 0x77;
    int retval;
    char *dev_path;

    if (argc < 2) {
        print_usage(argv[0]);
        return -1;
    }

    bus = strtol(argv[1], NULL, 10);

    if (argc >= 3)
        start_addr = strtol(argv[2], NULL, 16);

    if (argc >= 4)
        end_addr = strtol(argv[3], NULL, 16);

    retval = asprintf(&dev_path, "/dev/i2c-%d", bus);
    if (retval < 0)
        return retval;

    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "cannot open %s: %s\n", dev_path, strerror(-fd));
        fd = retval;
        goto error_open_device;
    }

    uint8_t buffer;
    struct i2c_msg msg = {
        .buffer = &buffer,
        .length = sizeof(buffer),
        .flags = I2C_M_READ,
    };

    printf("   ");
    for (unsigned i = 0; i <= 0xf; i++)
        printf("%2x ", i);
    printf("\n");

    for (unsigned addr = start_addr & ~0xf; addr <= end_addr;) {
        printf("%02x ", addr);
        for (unsigned i = 0; i <= 0xf; i++, addr++) {
            if (addr < start_addr || addr > end_addr) {
                printf("%3s", " ");
                continue;
            }

            msg.addr = addr;
            retval = ioctl(fd, I2C_TRANSFER, &msg, 1);
            if (retval < 0)
                printf("%3s", "--");
            else
                printf("%3x", addr);
        }
        printf("\n");
    }

    close(fd);

error_open_device:
    free(dev_path);

    return retval;
}

__shell_command__ struct shell_command i2cdetect_command = {
    .name = "i2cdetect",
    .description = "",
    .entry = i2cdetect_main,
};
