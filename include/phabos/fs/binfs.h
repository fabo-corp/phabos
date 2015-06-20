#ifndef __BINFS_H__
#define __BINFS_H__

struct bin_entry {
    void *bin;
    unsigned int *size;
    char name[64];
};

#endif /* __BINFS_H__ */

