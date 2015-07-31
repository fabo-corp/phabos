#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>

#include <phabos/kprintf.h>
#include <phabos/scheduler.h>
#include <phabos/syscall.h>
#include <phabos/fs.h>
#include <phabos/utils.h>
#include <phabos/driver.h>
#include <phabos/shell.h>
#include <phabos/mm.h>

#include <asm/delay.h>
#include <phabos/i2c.h>

__attribute__((unused)) static void ls(const char *path)
{
    int fd;
    int retval;
    char buffer[128];
    struct phabos_dirent *dirent = (struct phabos_dirent*) &buffer;

    fd = open(path, 0);
    if (fd < 0) {
        kprintf("open failed: %s\n", strerror(errno));
        return;
    }

    kprintf("%s:\n", path);
    do {
        retval = getdents(fd, dirent, ARRAY_SIZE(buffer));
        if (retval < 0) {
            kprintf("getdents failed: %s\n", strerror(errno));
            goto end;
        }

        for (int bpos = 0; bpos < retval;) {
            dirent = (struct phabos_dirent *) (buffer + bpos);
            char d_type = d_type = *(buffer + bpos + dirent->d_reclen - 1);

            kprintf("\t%s ", (d_type == DT_REG) ?  "regular" :
                               (d_type == DT_DIR) ?  "directory" :
                               (d_type == DT_FIFO) ? "FIFO" :
                               (d_type == DT_SOCK) ? "socket" :
                               (d_type == DT_LNK) ?  "symlink" :
                               (d_type == DT_BLK) ?  "block dev" :
                               (d_type == DT_CHR) ?  "char dev" : "???");
            kprintf("%.4d %s\n", dirent->d_reclen, dirent->d_name);
            bpos += dirent->d_reclen;
        }
    } while (retval > 0);

end:
    retval = close(fd);
    if (retval < 0)
        kprintf("close failed: %s\n", strerror(errno));
}

__attribute__((unused)) static void dev(void)
{
    int fd, fd2;
    int retval;
    char buffer[20];
    struct phabos_dirent *dirent = (struct phabos_dirent*) &buffer;

    retval = mkdir("/test/", 0);
    if (retval < 0)
        printf("mkdir failed: %s\n", strerror(errno));

    retval = mkdir("/toto/", 0);
    if (retval < 0)
        printf("mkdir failed: %s\n", strerror(errno));

    retval = mkdir("/test/tata", 0);
    if (retval < 0)
        printf("mkdir failed: %s\n", strerror(errno));

    retval = mkdir("/test/tata/tutu", 0);
    if (retval < 0)
        printf("mkdir failed: %s\n", strerror(errno));

    retval = mkdir("/toto/tata/tutu", 0);
    if (retval < 0)
        printf("mkdir failed: %s\n", strerror(errno));

    fd = open("/", 0);
    if (fd < 0)
        printf("open failed: %s\n", strerror(errno));
    else
        printf("allocated fd: %d\n", fd);

    fd2 = open("/file1", O_CREAT | O_EXCL);
    if (fd2 < 0)
        printf("open failed: %s\n", strerror(errno));
    else
        printf("allocated fd: %d\n", fd2);

    do {
        retval = getdents(fd, dirent, ARRAY_SIZE(buffer));
        if (retval < 0)
            printf("getdents failed: %s\n", strerror(errno));

        if (retval <= 0)
            break;

        for (int bpos = 0; bpos < retval;) {
            dirent = (struct phabos_dirent *) (buffer + bpos);
            char d_type = d_type = *(buffer + bpos + dirent->d_reclen - 1);

            printf("%-10s ", (d_type == DT_REG) ?  "regular" :
                             (d_type == DT_DIR) ?  "directory" :
                             (d_type == DT_FIFO) ? "FIFO" :
                             (d_type == DT_SOCK) ? "socket" :
                             (d_type == DT_LNK) ?  "symlink" :
                             (d_type == DT_BLK) ?  "block dev" :
                             (d_type == DT_CHR) ?  "char dev" : "???");
            printf("%4d %s\n", dirent->d_reclen, dirent->d_name);
            bpos += dirent->d_reclen;
        }
    } while (retval > 0);

    fd = close(fd);
    if (fd < 0)
        printf("close failed: %s\n", strerror(errno));
    else
        printf("closed fd\n");

    ssize_t nio;
    nio = write(fd2, "Hello World\n", 12);
    if (nio < 0)
        printf("could not write to file: %s\n", strerror(errno));
    else
        printf("wrote %d byte to file\n", nio);

    retval = lseek(fd2, SEEK_SET, 0);
    if (fd < 0)
        printf("lseek failed: %s\n", strerror(errno));

    nio = read(fd2, buffer, 13);
    if (nio < 0)
        printf("could not read file: %s\n", strerror(errno));
    else
        printf("read %d byte from file\n", nio);

    printf("%s", buffer);
}


#define STM32_I2C2_BASE 0x40005800

#define I2C_CR1     0x00
#define I2C_CR2     0x04
#define I2C_OAR1    0x08
#define I2C_OAR2    0x0c
#define I2C_DR      0x10
#define I2C_SR1     0x14
#define I2C_SR2     0x18
#define I2C_CCR     0x1c
#define I2C_TRISE   0x20
#define I2C_FLTR    0x24

#define I2C_CR1_PE          (1 << 0)
#define I2C_CR1_START       (1 << 8)
#define I2C_CR1_STOP        (1 << 9)

#define I2C_CR2_ITERREN     (1 << 8)
#define I2C_CR2_ITEVTEN     (1 << 9)

#define I2C_CCR_DUTY        (1 << 14)
#define I2C_CCR_FASTMODE    (1 << 15)

#include <asm/hwio.h>

struct semaphore i2c_semaphore;

static void i2c_dump(int line)
{
    kprintf("%d:\n", line);
    kprintf("\tCR1: %#X\n", read32(STM32_I2C2_BASE + I2C_CR1));
    kprintf("\tCR2: %#X\n", read32(STM32_I2C2_BASE + I2C_CR2));
    kprintf("\tSR1: %#X\n", read32(STM32_I2C2_BASE + I2C_SR1));
    kprintf("\tSR2: %#X\n", read32(STM32_I2C2_BASE + I2C_SR2));
    kprintf("\tTRISE: %#X\n", read32(STM32_I2C2_BASE + I2C_TRISE));
}

static void i2c_evt_irq(int irq, void *data)
{
    kprintf("IRQ EVT\n");
    i2c_dump(__LINE__);

    irq_clear(irq);
    irq_disable_line(irq);

    semaphore_up(&i2c_semaphore);
}

static void i2c_err_irq(int irq, void *data)
{
    kprintf("IRQ ERR\n");
    i2c_dump(__LINE__);
}

static void i2c_test(void)
{
    uint32_t val;

    kprintf("\r%c[2J",27);

    semaphore_init(&i2c_semaphore, 0);

    i2c_dump(__LINE__);

#define STM32_IRQ_I2C2_EV 33
#define STM32_IRQ_I2C2_ER 24

    irq_attach(STM32_IRQ_I2C2_EV, i2c_evt_irq, NULL);
    irq_enable_line(STM32_IRQ_I2C2_EV);

    irq_attach(STM32_IRQ_I2C2_ER, i2c_err_irq, NULL);
    irq_enable_line(STM32_IRQ_I2C2_ER);

    write32(STM32_I2C2_BASE + I2C_CR2, 42 | I2C_CR2_ITEVTEN | I2C_CR2_ITERREN);
    write32(STM32_I2C2_BASE + I2C_CCR, I2C_CCR_FASTMODE | /*I2C_CCR_DUTY |*/ 35);
    write32(STM32_I2C2_BASE + I2C_TRISE, 13);
    write32(STM32_I2C2_BASE + I2C_OAR1, (1 << 14));
    write32(STM32_I2C2_BASE + I2C_CR1, I2C_CR1_PE);

    i2c_dump(__LINE__);

    write32(STM32_I2C2_BASE + I2C_CR1, I2C_CR1_PE | I2C_CR1_START);

    semaphore_down(&i2c_semaphore);


    read32(STM32_I2C2_BASE + I2C_SR2) &= ~2;

//    while (read32(STM32_I2C2_BASE + I2C_SR2) & 2)
//        ;

    i2c_dump(__LINE__);

    val = read32(STM32_I2C2_BASE + I2C_SR1);

    write32(STM32_I2C2_BASE + I2C_DR, 0x20 << 1);
    val = read32(STM32_I2C2_BASE + I2C_SR1);
    val = read32(STM32_I2C2_BASE + I2C_SR2);

    i2c_dump(__LINE__);

//    while (read32(STM32_I2C2_BASE + I2C_SR2) & 2)
//        ;

    i2c_dump(__LINE__);

    for (int i = 0; i < 5; i++) {
        while (!(read32(STM32_I2C2_BASE + I2C_SR1) & (1 << 7)));
            write32(STM32_I2C2_BASE + I2C_DR, i);
    }

    write32(STM32_I2C2_BASE + I2C_CR1, I2C_CR1_PE | I2C_CR1_STOP);
    write32(STM32_I2C2_BASE + I2C_CR1, 0);
}


int dev_main(int argc, char **argv)
{
#ifdef CONFIG_BINFS
    extern struct fs binfs_fs;
    fs_register(&binfs_fs);
#endif


    i2c_test();

//    gb_gpio_register(3);

#if 0
//    dev();

//    int exec(const void *addr);
//    exec(&apps_test_test_elf);

    ls("/");

    retval = mkdir("/bin", 0);
    if (retval)
        kprintf("mkdir: %s\n", strerror(errno));

    ls("/bin");

    retval = mount(NULL, "/bin", "binfs", 0, NULL);
    if (retval < 0)
        kprintf("failed to mount binfs: %s\n", strerror(errno));

    ls("/bin");

    pid_t pid;
    char *argv2[] = {"test", NULL};
    char *envp[] = {NULL};

    retval = posix_spawn(&pid, "/bin/test", NULL, NULL,
                         (char**) &argv2, (char**) &envp);
    if (retval < 0)
        kprintf("spawn failed: %s\n", strerror(errno));

    kprintf("pid: %d\n", pid);
#endif

#if 0
    gpio_activate(0);
    kprintf("test:\n");

    while (1) {
        gpio_direction_out(0, 1);
        udelay(1);
        gpio_direction_out(0, 0);
        udelay(1);
    }
#endif

    ls("/dev");
#if 0
    int fd = open("/dev/i2c-0", 0);

    struct i2c_msg {
        uint16_t addr;
        uint8_t *buffer;
        size_t length;
        unsigned long flags;
    };

    char buffer[] = {0, 1, 2, 3, 4};

    struct i2c_msg msg = {
        .addr = 0x20,
        .buffer = &buffer,
        .length = ARRAY_SIZE(buffer),
    };

    ioctl(fd, I2C_SET_FREQUENCY, 400000);
    ioctl(fd, I2C_TRANSFER, &msg, 1);

    close(fd);
#endif

#if defined(CONFIG_TSB_APB1)
    bridge_main(argc, argv);
#endif

    int shell_main(int argc, char **argv);
    shell_main(argc, argv);
    return 0;
}
