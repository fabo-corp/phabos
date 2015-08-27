#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include <apps/shell.h>
#include <phabos/scheduler.h>

enum {
    CHAR_EOT = 0x3,
};

static void term_main_in(void *data)
{
    char buffer[256];

    int fd_in = open(data, O_RDONLY);
    int fd_out = open("/dev/ttyS0", O_RDWR);

    while (1) {
        ssize_t nread = read(fd_in, buffer, 256);
        if (nread > 0) {
            write(fd_out, buffer, nread);
        }
    }
}

static int term_main(int argc, char **argv)
{
    struct task *task_in;

    if (argc != 2)
        return -1;

    int stdio = open("/dev/ttyS0", O_RDWR);
    int fd_out = open("/dev/ttyS1", O_RDWR);

    if (fd_out < 0) {
        fprintf(stderr, "cannot open %s: %s\n", argv[1], strerror(-fd_out));
        return fd_out;
    }

    task_in = task_run("term-in", term_main_in, argv[1], 0);

    while (1) {
        int c = getchar();
        if (c == CHAR_EOT) {
            printf("\n");
            return 0;
        } else {
            write(stdio, &c, 1);
            write(fd_out, &c, 1);
        }
    }

    task_kill(task_in);

    return 0;
}

__shell_command__ struct shell_command term_command = {
    .name = "term",
    .description = "",
    .entry = term_main,
};
