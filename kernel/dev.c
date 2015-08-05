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
#include <phabos/mm.h>
#include <phabos/i2c.h>

#include <apps/shell.h>
#include <asm/delay.h>

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

void __aeabi_memclr4(void *dest, size_t n)
{
    memset(dest, 0, n);
}

int dev_main(int argc, char **argv)
{
#ifdef CONFIG_BINFS
    extern struct fs binfs_fs;
    fs_register(&binfs_fs);
#endif


//    i2c_test();

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

#if defined(CONFIG_TSB_APB1)
    bridge_main(argc, argv);
#endif

    int shell_main(int argc, char **argv);
    shell_main(argc, argv);
    return 0;
}
