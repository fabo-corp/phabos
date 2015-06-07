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

static void ls(const char *path)
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

int dev_main(int argc, char **argv)
{
    int retval;

    extern struct fs ramfs_fs;
    extern struct fs devfs_fs;
    extern struct fs binfs_fs;

    fs_register(&ramfs_fs);
    fs_register(&devfs_fs);
    fs_register(&binfs_fs);

    retval = mount(NULL, NULL, "ramfs", 0, NULL);
    if (retval < 0)
        kprintf("failed to mount the ramfs: %s\n", strerror(errno));

    retval = mkdir("/dev", 0);
    if (retval)
        kprintf("mkdir: %s\n", strerror(errno));

    ls("/dev");

    device_driver_probe_all();

    ls("/dev");

    retval = mount(NULL, "/dev", "devfs", 0, NULL);
    if (retval < 0)
        kprintf("failed to mount devfs: %s\n", strerror(errno));

    ls("/dev");

    open("/dev/ttyS0", 0);
    open("/dev/ttyS0", 0);
    open("/dev/ttyS0", 0);
    write(1, "Hello Serial Driver\n", 20);

    FILE *fd = fopen("/dev/ttyS0", "r+");
    fprintf(fd, "Yop\n");
    fclose(fd);

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

    int shell_main(int argc, char **argv);
    shell_main(argc, argv);
    return 0;
}
