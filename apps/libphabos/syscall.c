int write(int fd, const char *str, unsigned size)
{
    asm volatile(
        "push {r7};"
        "mov r7, #4;"
        "svc 0;"
        "pop {r7};"
    );

    return 0;
}

int write3(int fd)
{
    asm volatile(
        "push {r7};"
        "mov r7, #4;"
        "svc 0;"
        "pop {r7};"
    );

    return 0;
}

int write4(int fd)
{
    asm volatile(
        "push {r7};"
        "mov r7, #4;"
        "svc 0;"
        "pop {r7};"
    );

    return 0;
}
