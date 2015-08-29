/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __MM_H__
#define __MM_H__

#include <stdlib.h>
#include <string.h>

#include <asm/atomic.h>
#include <phabos/list.h>

#define PAGE_ORDER 8
#define PAGE_SIZE (1 << PAGE_ORDER)

#define MIN_REGION_ORDER PAGE_ORDER

#define MM_DMA          (1 << 0)
#define MM_KERNEL       (1 << 1)

struct mcache;

typedef void (*mcache_constructor_t)(void *ptr);
typedef void (*mcache_destructor_t)(void *ptr);

struct mm_region {
    uintptr_t start;
    size_t size;
    unsigned long flags;
    struct list_head list;
};

struct mm_usage {
    atomic_t total;
    atomic_t used;
    atomic_t cached;
};

void mm_init(unsigned long flags);
int mm_add_region(struct mm_region *region);

void *kmalloc(size_t size, unsigned int flags);
void kfree(void *ptr);
void *page_alloc(unsigned int flags, int order);
void page_free(void *ptr, int order);

int size_to_order(size_t size);

void *mcache_alloc(struct mcache *cache);
void mcache_free(struct mcache *cache, void *ptr);
struct mcache *mcache_create(const char *const name, size_t size, size_t align,
                             unsigned long flags, mcache_constructor_t ctor,
                             mcache_destructor_t dtor);
void mcache_destroy(struct mcache *cache);

struct mm_usage *mm_get_usage(void);

static inline void *kzalloc(size_t size, unsigned int flags)
{
    void *ptr = kmalloc(size, flags);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

static inline void *zalloc(size_t size)
{
    return kzalloc(size, MM_KERNEL);
}

#endif /* __MM_H__ */

