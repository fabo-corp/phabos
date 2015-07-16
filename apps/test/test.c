int write(int fd, const char *str, unsigned size);
int write3(int fd);
int write4(int fd);

static int write2(int fd, const char *str, unsigned size)
{
    asm volatile(
        "push {r7};"
        "mov r7, #4;"
        "svc 0;"
        "pop {r7};"
    );

    return 0;
}

int main(int argc, char **argv)
{
#if 1
    write(1, "Hello From Apps\n", 16);
    write3(2);
    write4(2);
#endif

    write2(2, "Hello From Apps\n", 16);

    return 0;
}
