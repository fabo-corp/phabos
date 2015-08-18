#include <stdio.h>

#include <phabos/mm.h>
#include <apps/shell.h>

static int free_main(int argc, char **argv)
{
    unsigned long total;
    unsigned long used;
    unsigned long cached;
    unsigned long free;

    struct mm_usage *usage = mm_get_usage();

    total = (unsigned long) atomic_get(&usage->total);
    used = (unsigned long) atomic_get(&usage->used);
    cached = (unsigned long) atomic_get(&usage->cached);
    free = total - used;

    printf("      %12s %12s %12s %12s\n", "total", "used", "free", "cache");
    printf("Mems: %12lu %12lu %12lu %12lu\n", total, used, free, cached);

    return 0;
}

__shell_command__ struct shell_command free_command = {
    .name = "free",
    .description = "Get memory usage statistics",
    .entry = free_main,
};
