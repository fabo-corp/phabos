#include <stdio.h>
#include <errno.h>

#include <apps/shell.h>
#include <phabos/fs.h>

static int ls(const char *path)
{
    int fd;
    int retval;
    char buffer[128];
    struct phabos_dirent *dirent = (struct phabos_dirent*) &buffer;

    fd = open(path, 0);
    if (fd < 0) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return fd;
    }

    do {
        retval = getdents(fd, dirent, ARRAY_SIZE(buffer));
        if (retval < 0) {
            fprintf(stderr, "getdents failed: %s\n", strerror(errno));
            goto end;
        }

        for (int bpos = 0; bpos < retval;) {
            dirent = (struct phabos_dirent *) (buffer + bpos);
            char d_type = d_type = *(buffer + bpos + dirent->d_reclen - 1);

            printf("\t%s ", (d_type == DT_REG) ?  "regular" :
                               (d_type == DT_DIR) ?  "directory" :
                               (d_type == DT_FIFO) ? "FIFO" :
                               (d_type == DT_SOCK) ? "socket" :
                               (d_type == DT_LNK) ?  "symlink" :
                               (d_type == DT_BLK) ?  "block dev" :
                               (d_type == DT_CHR) ?  "char dev" : "???");
            printf("%.4d %s\n", dirent->d_reclen, dirent->d_name);
            bpos += dirent->d_reclen;
        }
    } while (retval > 0);

end:
    retval = close(fd);
    if (retval < 0)
        fprintf(stderr, "close failed: %s\n", strerror(errno));
    return retval;
}

int ls_main(int argc, char **argv)
{
    return ls(argc < 2 ? "/" : argv[1]);
}

__shell_command__ struct shell_command ls_command = {
    .name = "ls",
    .description = "List directory entries",
    .entry = ls_main,
};
