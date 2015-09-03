/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <asm/spinlock.h>

#include <phabos/mm.h>
#include <phabos/list.h>

struct mcache_slab {
    struct mcache *cache;
    struct list_head list;
    size_t size;
    uintptr_t buffer;
    unsigned refcount;
    uint8_t bitmap[0];
};

struct mcache {
    const char *name;
    size_t size;
    unsigned long flags;

    mcache_constructor_t ctor;
    mcache_destructor_t dtor;

    struct list_head empty_slabs_list;
    struct list_head partial_slabs_list;
    struct list_head full_slabs_list;

    struct spinlock lock;
};

struct mcache *mcache_create(const char *const name, size_t size, size_t align,
                             unsigned long flags, mcache_constructor_t ctor,
                             mcache_destructor_t dtor)
{
    struct mcache *cache;

    cache = kzalloc(sizeof(*cache), 0);
    if (!cache)
        return NULL;

    cache->name = name;
    /*
     * XXX: if size is larger than page size it won't work, to have it to work,
     * we need to allocate one extra page and just "throw away" the first page
     * or last page.
     */
    cache->size = MAX(size, align);
    cache->flags = flags;
    cache->ctor = ctor;
    cache->dtor = dtor;

    spinlock_init(&cache->lock);
    list_init(&cache->empty_slabs_list);
    list_init(&cache->partial_slabs_list);
    list_init(&cache->full_slabs_list);

    return cache;
}

static void mcache_destroy_slab(struct mcache_slab *slab)
{
    const size_t num_object = slab->size / slab->cache->size;
    void *buffer;
    struct mm_usage *mm_usage;

    if (slab->cache->dtor) {
        for (unsigned i = 0; i < num_object; i++) {
            buffer = (void*) (slab->buffer + slab->cache->size * i);
            slab->cache->dtor(buffer);
        }
    }

    mm_usage = mm_get_usage();
    if (mm_usage)
        atomic_add(&mm_usage->cached, -size_to_order(slab->size));

    page_free((void*) slab->buffer, size_to_order(slab->size));
    kfree(slab);
}

void mcache_destroy(struct mcache *cache)
{
    if (!cache)
        return;

    list_foreach(&cache->full_slabs_list, iter)
        mcache_destroy_slab(list_entry(iter, struct mcache_slab, list));

    list_foreach(&cache->partial_slabs_list, iter)
        mcache_destroy_slab(list_entry(iter, struct mcache_slab, list));

    list_foreach(&cache->empty_slabs_list, iter)
        mcache_destroy_slab(list_entry(iter, struct mcache_slab, list));

    kfree(cache);
}

static void *slab_alloc_object(struct mcache_slab *slab)
{
    const size_t num_object = slab->size / slab->cache->size;
    void *buffer = NULL;

    // XXX: force num_object per slab to be a multiple of 8 to avoid checking
    //      bit after bit and check the whole word?
    for (unsigned i = 0; i < num_object; i++) {
        unsigned byte = i / 8;
        unsigned bit = i % 8;

        if (slab->bitmap[byte] & (1 << bit))
            continue;

        slab->bitmap[byte] |= 1 << bit;
        buffer = (void*) (slab->buffer + slab->cache->size * i);
        slab->refcount++;
        break;
    }

    if (slab->refcount == num_object) {
        list_del(&slab->list);
        list_add(&slab->cache->full_slabs_list, &slab->list);
    }

    return buffer;
}

static struct mcache_slab *mcache_allocate_new_slab(struct mcache *cache)
{
    const size_t num_objects = 5; // XXX: find an algorith that will find
                                   // a good value for this
    struct mm_usage *mm_usage;

    void *buffer;
    struct mcache_slab *slab;
    size_t bitmap_size = num_objects / 8;
    if (num_objects % 8)
        bitmap_size++;

    size_t buffer_size = num_objects * cache->size;
    size_t pages = buffer_size / PAGE_SIZE;
    if (buffer_size % PAGE_SIZE)
        pages++;

    buffer = page_alloc(cache->flags, size_to_order(pages));
    if (!buffer)
        return NULL;

    mm_usage = mm_get_usage();
    if (mm_usage)
        atomic_add(&mm_usage->cached, buffer_size);

    slab = kzalloc(sizeof(*slab) + bitmap_size, 0);
    if (!slab)
        return NULL;

    slab->cache = cache;
    slab->size = buffer_size;
    slab->buffer = (uintptr_t) buffer;
    list_init(&slab->list);

    if (cache->ctor) {
        for (int i = 0; i < num_objects; i++)
            cache->ctor((char *) buffer + i * cache->size);
    }

    return slab;
}

static struct mcache_slab*
mcache_move_empty_slab_to_partial_list(struct mcache *cache)
{
    struct mcache_slab *slab;

    if (list_is_empty(&cache->empty_slabs_list)) {
        slab = mcache_allocate_new_slab(cache);
        if (!slab)
            return NULL;
    } else {
        slab = list_first_entry(&cache->empty_slabs_list, struct mcache_slab,
                                list);
    }

    list_del(&slab->list);
    list_add(&cache->partial_slabs_list, &slab->list);
    return slab;
}

void *mcache_alloc(struct mcache *cache)
{
    struct mcache_slab *slab;
    void *buffer = NULL;

    if (!cache)
        return NULL;

    spinlock_lock(&cache->lock);

    if (list_is_empty(&cache->partial_slabs_list)) {
        slab = mcache_move_empty_slab_to_partial_list(cache);
    } else {
        slab = list_first_entry(&cache->partial_slabs_list, struct mcache_slab,
                                list);
    }

    if (!slab)
        goto out;

    buffer = slab_alloc_object(slab);

out:
    spinlock_unlock(&cache->lock);
    return buffer;
}

static void slab_free_object(struct mcache_slab *slab, void *buffer)
{
    const uintptr_t offset = (uintptr_t) buffer - slab->buffer;
    const unsigned object = offset / slab->cache->size;
    uint8_t byte = object / 8;
    uint8_t bit = object % 8;

    slab->bitmap[byte] &= ~(1 << bit);
    slab->refcount--;

    /*
     * no op if slab is already in partial_slabs_list, otherwise move slab
     * to partial slab list
     */
    list_del(&slab->list);
    list_add(&slab->cache->partial_slabs_list, &slab->list);

    /* slab is now empty, move it to the empty slab list */
    if (!slab->refcount) {
        list_del(&slab->list);
        list_add(&slab->cache->empty_slabs_list, &slab->list);
    }
}

struct mcache_slab *find_buffer_slab_in_list(struct mcache *cache, void *buffer,
                                             struct list_head *head)
{
    uintptr_t ptr = (uintptr_t) buffer;

    list_foreach(head, iter) {
        struct mcache_slab *slab = list_entry(iter, struct mcache_slab, list);

        if (ptr >= slab->buffer && ptr < slab->buffer + slab->size)
            return slab;
    }

    return NULL;
}

struct mcache_slab *find_buffer_slab(struct mcache *cache, void *buffer)
{
    struct mcache_slab *slab;

    slab = find_buffer_slab_in_list(cache, buffer, &cache->partial_slabs_list);
    if (slab)
        return slab;

    return find_buffer_slab_in_list(cache, buffer, &cache->full_slabs_list);
}

void mcache_free(struct mcache *cache, void *ptr)
{
    struct mcache_slab *slab;

    if (!cache || !ptr)
        return;

    spinlock_lock(&cache->lock);

    slab = find_buffer_slab(cache, ptr);
    if (!slab)
        goto out;

    slab_free_object(slab, ptr);

out:
    spinlock_unlock(&cache->lock);
}
