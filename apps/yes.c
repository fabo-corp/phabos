#include <stdio.h>

#include <apps/shell.h>

int yes_main(int argc, char **argv)
{
    while (1)
        printf("yes\n");

    return 0;
}

__shell_command__ struct shell_command yes_command = {
    .name = "yes",
    .description = "",
    .entry = yes_main,
};
