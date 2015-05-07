#include <phabos/assert.h>
#include <phabos/mm.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

char *astrcpy(const char *str)
{
    char *copy;

    copy = kmalloc(strlen(str) + 1, 0);
    strcpy(copy, str);

    return copy;
}

char *adirname(const char *path)
{
    int i;
    char *dirname;
    size_t path_size;

    RET_IF_FAIL(path, NULL);

    path_size = strlen(path);
    RET_IF_FAIL(path_size > 0, NULL);

    for (i = path_size - 1; i >= 0; i--) {
        if (path[i] != '/')
            break;
    }

    /* path is only filled of '/' */
    if (i < 0)
        return astrcpy("/");

    for (; i >= 0; i--) {
        if (path[i] == '/')
            break;
    }

    /* path does not contain any '/' */
    if (i < 0)
        return astrcpy(".");

    /* path is of the following form /abc */
    if (i == 0)
        return astrcpy("/");

    dirname = kmalloc(i + 1, 0);
    RET_IF_FAIL(dirname, NULL);

    memcpy(dirname, path, i);
    dirname[i] = '\0';

    return dirname;
}

char *abasename(const char *path)
{
    int i;
    int j;
    char *dirname;
    size_t path_size;

    RET_IF_FAIL(path, NULL);

    path_size = strlen(path);
    RET_IF_FAIL(path_size > 0, NULL);

    for (i = path_size - 1; i >= 0; i--) {
        if (path[i] != '/')
            break;
    }

    /* path is only filled of '/' */
    if (i < 0)
        return astrcpy("/");

    for (j = i; j >= 0; j--) {
        if (path[j] == '/')
            break;
    }

    i += 1;
    j += 1;

    dirname = kmalloc(i - j + 1, 0);
    RET_IF_FAIL(dirname, NULL);

    memcpy(dirname, path + j, i - j);
    dirname[i - j] = '\0';

    return dirname;
}
