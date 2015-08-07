#include <asm/spinlock.h>
#include <phabos/assert.h>
#include <phabos/list.h>
#include <phabos/mm.h>
#include <phabos/utils.h>

#include <errno.h>

#define PAGE_ORDER 12
#define PAGE_SIZE (1 << PAGE_ORDER)
#define MIN_REGION_ORDER PAGE_ORDER
#define MAX_ADDRESSABLE_MEM_ORDER 31

static struct spinlock mm_lock = SPINLOCK_INIT(mm_lock);
static struct list_head mm_bucket[MAX_ADDRESSABLE_MEM_ORDER + 1];
static struct list_head mm_region_list = LIST_INIT(mm_region_list);
static bool is_initialized;

struct mm_buffer {
    uint32_t bucket;
    struct mm_region *region;
    struct list_head list;
} __attribute__((packed)); // MUST be a multiple of 8 bytes

static size_t order_to_size(int order)
{
    return 1 << order;
}

static size_t count_bit_set(unsigned long val)
{
    size_t bit_set_count = 0;

    for (; val; val >>= 1) {
        if (val & 1)
            bit_set_count++;
    }

    return bit_set_count;
}

int size_to_order(size_t size)
{
    int order = -1;
    size_t bit_set_count = 0;

    for (; size; size >>= 1) {
        order++;

        if (size & 1)
            bit_set_count++;
    }

    return bit_set_count == 1 ? order : order + 1;
}

int mm_add_region(struct mm_region *region)
{
    struct mm_buffer *buffer;

#if 0
    RET_IF_FAIL(order >= MIN_REGION_ORDER, -EINVAL);
    RET_IF_FAIL(addr >= PAGE_SIZE, -EINVAL);
    RET_IF_FAIL(!(addr & 0x3), -EINVAL);
    RET_IF_FAIL(order < MAX_ADDRESSABLE_MEM_ORDER, -EINVAL);
#endif

    if (count_bit_set(region->size)) {
        kprintf("mm: rejecting memory region with size that is not a power of 2, size: %u",
                region->size);
        return -EINVAL;
    }

    spinlock_lock(&mm_lock);

    if (!is_initialized) {
        for (int i = 0; i < ARRAY_SIZE(mm_bucket); i++)
            list_init(&mm_bucket[i]);

        is_initialized = true;
    }

    list_init(&region->list);
    list_add(&mm_region_list, &region->list);

    buffer = (struct mm_buffer*) region->start;
    buffer->bucket = size_to_order(region->size);
    buffer->region = region;
    list_init(&buffer->list);

    list_add(&mm_bucket[buffer->bucket], &buffer->list);

    spinlock_unlock(&mm_lock);
    return 0;
}

static struct mm_buffer *find_buffer_in_bucket(int order, unsigned int flags)
{
    struct mm_buffer *buffer;

    if (list_is_empty(&mm_bucket[order]))
        return NULL;

    list_foreach(&mm_bucket[order], iter) {
        buffer = list_entry(iter, struct mm_buffer, list);
        if (buffer->region->flags == flags)
            return buffer;
    }

    return NULL;
}

static int fill_bucket(int order, unsigned int flags)
{
    struct mm_buffer *buffer1;
    struct mm_buffer *buffer2;
    int retval;

    if (order >= 15)
        return -ENOMEM;

    buffer1 = find_buffer_in_bucket(order + 1, flags);
    if (!buffer1) {
        retval = fill_bucket(order + 1, flags);
        if (retval)
            return retval;

        buffer1 = find_buffer_in_bucket(order + 1, flags);
        RET_IF_FAIL(buffer1, -ENOMEM);
    }

    list_del(&buffer1->list);

    buffer2 = (struct mm_buffer*) ((char*) buffer1 + order_to_size(order));
    buffer2->region = buffer1->region;
    list_init(&buffer2->list);

    buffer1->bucket = order;
    buffer2->bucket = order;

    list_add(&mm_bucket[order], &buffer1->list);
    list_add(&mm_bucket[order], &buffer2->list);

    return 0;
}

static void defragment(struct mm_buffer *buffer)
{
    struct mm_buffer *buffer_low;
    struct mm_buffer *buffer_high;

    struct mm_buffer *buffer2;
    struct mm_buffer *buffer3;

    RET_IF_FAIL(!list_is_empty(&mm_bucket[buffer->bucket]),);

    if (((unsigned long) buffer) & (1 << buffer->bucket)) {
        buffer_low = buffer2 = (struct mm_buffer*)
            ((char*) buffer - order_to_size(buffer->bucket));
        buffer_high = buffer;
    } else {
        buffer_low = buffer;
        buffer_high = buffer2 = (struct mm_buffer*)
            ((char*) buffer + order_to_size(buffer->bucket));
    }

    list_foreach(&mm_bucket[buffer->bucket], iter) {
        buffer3 = list_entry(iter, struct mm_buffer, list);
        if (buffer3 != buffer2)
            continue;

        if (buffer_low->region != buffer_high->region)
            return;

        list_del(&buffer_low->list);
        list_del(&buffer_high->list);

        buffer_low->bucket++;
        list_add(&mm_bucket[buffer_low->bucket], &buffer_low->list);

        defragment(buffer_low);

        return;
    }
}

static struct mm_buffer *get_buffer(int order, unsigned int flags)
{
    struct mm_buffer *buffer;
    int retval;

    buffer = find_buffer_in_bucket(order, flags);
    if (buffer)
        return buffer;

    retval = fill_bucket(order, flags);
    if (!retval) {
        buffer = find_buffer_in_bucket(order, flags);
        RET_IF_FAIL(buffer, NULL);
        return buffer;
    }

    if (flags & MM_DMA)
        return NULL;

    return get_buffer(order, MM_DMA);
}

void *kmalloc(size_t size, unsigned int flags)
{
    int order;
    struct mm_buffer *buffer;

    if (!size)
        return NULL;

    if (!is_initialized) {
        return malloc(size);
    }

    size += sizeof(*buffer);

#if 0
    static size_t total_size = 0;
    total_size += size;
    kprintf("%s(%u) = %u\n", __func__, size, total_size);
#endif

    order = size_to_order(size);
    if (order < 0)
        return NULL;

    if (order > MAX_ADDRESSABLE_MEM_ORDER)
        return NULL;

    spinlock_lock(&mm_lock);

    buffer = get_buffer(order, flags);
    if (!buffer)
        goto error;

    list_del(&buffer->list);

    spinlock_unlock(&mm_lock);

    return (char*) buffer + sizeof(*buffer);

error:
    spinlock_unlock(&mm_lock);
    return NULL;
}

void kfree(void *ptr)
{
    struct mm_buffer *buffer;

    if (!ptr)
        return;

    if (!is_initialized) {
        free(ptr);
        return;
    }

    buffer = (struct mm_buffer*) ((char *) ptr - sizeof(*buffer));
    RET_IF_FAIL(buffer->list.prev == buffer->list.next,);

    spinlock_lock(&mm_lock);

    list_add(&mm_bucket[buffer->bucket], &buffer->list);
    defragment(buffer);

    spinlock_unlock(&mm_lock);
}

void *page_alloc(unsigned int flags, int order)
{
    size_t size = ((1 << order) << PAGE_ORDER) - sizeof(struct mm_buffer);
    char *buffer = kmalloc(size, flags);
    return buffer - sizeof(struct mm_buffer);
}

void page_free(void *ptr, int order)
{
    struct mm_buffer *buffer = ptr;
    struct mm_region *region = NULL;
    uintptr_t ptraddr = (uintptr_t) ptr;
    size_t size = (1 << order) * PAGE_SIZE;

    spinlock_lock(&mm_lock);

    list_foreach(&mm_region_list, iter) {
        struct mm_region *reg = list_entry(iter, struct mm_region, list);
        if (ptraddr >= reg->start && ptraddr < reg->start + reg->size) {
            region = reg;
            break;
        }
    }

    spinlock_unlock(&mm_lock);

    if (!region) {
        kprintf("mm: trying to free an invalid memory region\n");
        return;
    }

    if (ptraddr + size > region->start + region->size) {
        kprintf("mm: trying to free a page that cross memory regions\n");
        return;
    }

    list_init(&buffer->list);
    buffer->bucket = size_to_order(size);
    buffer->region = region;

    kfree((char *) buffer + sizeof(*buffer));
}
