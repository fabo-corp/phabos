/*
 * Copyright (c) 2014-2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 */

#include <phabos/assert.h>
#include <phabos/greybus.h>
#include <phabos/scheduler.h>
#include <phabos/time.h>
#include <phabos/watchdog.h>
#include <phabos/greybus/debug.h>
#include <phabos/greybus/tape.h>
#include <phabos/unipro/unipro.h>

#include <asm/spinlock.h>
#include <asm/byteordering.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define TYPE_RESPONSE_FLAG      0x80
#define TIMEOUT_IN_MS           1000
#define GB_INVALID_TYPE         0

#define ONE_SEC_IN_MSEC         1000
#define ONE_MSEC_IN_NSEC        1000000

static struct gb_operation_hdr timedout_hdr = {
    .size = sizeof(timedout_hdr),
    .result = GB_OP_TIMEOUT,
    .type = TYPE_RESPONSE_FLAG,
};
static struct gb_operation_hdr oom_hdr = {
    .size = sizeof(timedout_hdr),
    .result = GB_OP_NO_MEMORY,
    .type = TYPE_RESPONSE_FLAG,
};

struct gb_cport {
    struct greybus *bus;
    unsigned id;
    struct gb_driver *driver;
    struct list_head tx_fifo;
    struct spinlock tx_fifo_lock;
    struct spinlock rx_fifo_lock;
    struct list_head rx_fifo;
    struct semaphore rx_fifo_semaphore;
    struct task *thread;
    struct watchdog timeout_wd;
    struct gb_operation timedout_operation;
};

struct gb_tape_record_header {
    uint16_t size;
    uint16_t cport;
};

static void gb_operation_timeout(struct watchdog *wdog);

uint8_t gb_errno_to_op_result(int err)
{
    switch (err) {
    case 0:
        return GB_OP_SUCCESS;

    case ENOMEM:
    case -ENOMEM:
        return GB_OP_NO_MEMORY;

    case EINTR:
    case -EINTR:
        return GB_OP_INTERRUPTED;

    case ETIMEDOUT:
    case -ETIMEDOUT:
        return GB_OP_TIMEOUT;

    case EPROTO:
    case -EPROTO:
    case ENOSYS:
    case -ENOSYS:
        return GB_OP_PROTOCOL_BAD;

    case EINVAL:
    case -EINVAL:
        return GB_OP_INVALID;

    case EOVERFLOW:
    case -EOVERFLOW:
        return GB_OP_OVERFLOW;

    case ENODEV:
    case -ENODEV:
    case ENXIO:
    case -ENXIO:
        return GB_OP_NONEXISTENT;

    case EBUSY:
    case -EBUSY:
        return GB_OP_RETRY;

    default:
        return GB_OP_UNKNOWN_ERROR;
    }
}

static int gb_compare_handlers(const void *data1, const void *data2)
{
    const struct gb_operation_handler *handler1 = data1;
    const struct gb_operation_handler *handler2 = data2;
    return (int)handler1->type - (int)handler2->type;
}

static struct gb_cport *gb_get_cport(struct greybus *bus, unsigned cportid)
{
    return hashtable_get(bus->cport_map, (void*) cportid);
}

static struct gb_operation_handler*
find_operation_handler(struct greybus *greybus, uint8_t type, unsigned cportid)
{
    struct gb_cport *cport = gb_get_cport(greybus, cportid);
    struct gb_driver *driver = cport->driver;
    int l,r;

    if (type == GB_INVALID_TYPE || !driver->op_handlers) {
        return NULL;
    }

    /* binary search -- bsearch from libc is not implemented by nuttx */
    l = 0;
    r = driver->op_handlers_count - 1;
    while (l <= r) {
        int m = (l + r) / 2;
        if (driver->op_handlers[m].type < type)
            l = m + 1;
        else if (driver->op_handlers[m].type > type)
            r = m - 1;
        else
            return &driver->op_handlers[m];
    }

    return NULL;
}

static void gb_process_request(struct gb_operation_hdr *hdr,
                               struct gb_operation *operation)
{
    struct greybus *greybus = operation->greybus;
    struct gb_operation_handler *op_handler;
    uint8_t result;

    op_handler = find_operation_handler(greybus, hdr->type, operation->cport);
    if (!op_handler) {
        gb_error("Cport %u: Invalid operation type %u\n",
                 operation->cport, hdr->type);
        gb_operation_send_response(operation, GB_OP_INVALID);
        return;
    }

    result = op_handler->handler(operation);

    if (hdr->id)
        gb_operation_send_response(operation, result);
}

static bool gb_operation_has_timedout(struct gb_operation *operation)
{
    struct timespec current_time;
    struct timespec timeout_time;

    timeout_time.tv_sec = operation->time.tv_sec +
                          TIMEOUT_IN_MS / ONE_SEC_IN_MSEC;
    timeout_time.tv_nsec = operation->time.tv_nsec +
                          (TIMEOUT_IN_MS % ONE_SEC_IN_MSEC) * ONE_MSEC_IN_NSEC;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    if (current_time.tv_sec > timeout_time.tv_sec)
        return true;

    if (current_time.tv_sec < timeout_time.tv_sec)
        return false;

    return current_time.tv_nsec > timeout_time.tv_nsec;
}

/**
 * Update watchdog state
 *
 * Cancel cport watchdog if there is no outgoing message waiting for a response,
 * or update the watchdog if there is still outgoing messages.
 *
 * TODO use fine-grain timeout delay when doing the update
 *
 * @note This function should be called from an atomic context
 */
static void gb_watchdog_update(struct gb_cport *cport)
{
    irq_disable();

    if (list_is_empty(&cport->tx_fifo)) {
        watchdog_cancel(&cport->timeout_wd);
    } else {
        watchdog_start_msec(&cport->timeout_wd, TIMEOUT_IN_MS);
    }

    irq_enable();
}

static void gb_clean_timedout_operation(struct gb_cport *cport)
{
    struct gb_operation *op;

    list_foreach_safe(&cport->tx_fifo, iter) {
        op = list_entry(iter, struct gb_operation, list);

        if (!gb_operation_has_timedout(op)) {
            continue;
        }

        irq_disable();
        list_del(iter);
        irq_enable();

        if (op->callback) {
            op->callback(op);
        }
        gb_operation_unref(op);
    }

    gb_watchdog_update(cport);
}

static void gb_process_response(struct gb_operation_hdr *hdr,
                                struct gb_operation *operation)
{
    struct greybus *greybus = operation->greybus;
    struct gb_cport *cport = gb_get_cport(greybus, operation->cport);
    struct gb_operation *op;
    struct gb_operation_hdr *op_hdr;

    list_foreach_safe(&cport->tx_fifo, iter) {
        op = list_entry(iter, struct gb_operation, list);
        op_hdr = op->request_buffer;

        if (hdr->id != op_hdr->id)
            continue;

        spinlock_lock(&cport->tx_fifo_lock);
        list_del(iter);
        gb_watchdog_update(cport);
        spinlock_unlock(&cport->tx_fifo_lock);

        /* attach this response with the original request */
        gb_operation_ref(operation);
        op->response = operation;
        if (op->callback)
            op->callback(op);
        gb_operation_unref(op);
        break;
    }
}

static void gb_pending_message_worker(void *data)
{
    struct gb_cport *cport = data;
    struct gb_operation *operation;
    struct list_head *head;
    struct gb_operation_hdr *hdr;

    while (1) {
        semaphore_lock(&cport->rx_fifo_semaphore);

        spinlock_lock(&cport->rx_fifo_lock);
        head = cport->rx_fifo.next;
        list_del(cport->rx_fifo.next);
        spinlock_unlock(&cport->rx_fifo_lock);

        operation = list_entry(head, struct gb_operation, list);
        hdr = operation->request_buffer;

        if (hdr == &timedout_hdr) {
            gb_clean_timedout_operation(cport);
            continue;
        }

        if (hdr->type & TYPE_RESPONSE_FLAG)
            gb_process_response(hdr, operation);
        else
            gb_process_request(hdr, operation);
        gb_operation_destroy(operation);
    }
}

static void greybus_rx_handler(struct unipro_cport_driver *cport_driver,
                               unsigned cportid, void *data, size_t size)
{
    struct greybus *greybus =
        containerof(cport_driver, struct greybus, unipro_cport_driver);
    struct gb_cport *cport = gb_get_cport(greybus, cportid);
    struct gb_operation *op;
    struct gb_operation_hdr *hdr = data;
    struct gb_operation_handler *op_handler;
    size_t hdr_size;

    if (!cport || !data)
        return;

    if (!cport->driver || !cport->driver->op_handlers)
        return;

    if (sizeof(*hdr) > size) {
        dev_error(&greybus->device, "Dropping garbage request\n");
        return; /* Dropping garbage request */
    }

    hdr_size = le16_to_cpu(hdr->size);

    if (hdr_size > size || sizeof(*hdr) > hdr_size) {
        dev_error(&greybus->device, "Dropping garbage request\n");
        return; /* Dropping garbage request */
    }

    gb_dump(data, size);

    if (greybus->tape && greybus->tape_fd >= 0) {
        struct gb_tape_record_header record_hdr = {
            .size = size,
            .cport = cportid,
        };

        greybus->tape->write(greybus->tape_fd, &record_hdr, sizeof(record_hdr));
        greybus->tape->write(greybus->tape_fd, data, size);
    }

    op_handler = find_operation_handler(greybus, hdr->type, cportid);
    if (op_handler && op_handler->fast_handler) {
        op_handler->fast_handler(cportid, data);
        return;
    }

    op = gb_operation_create(greybus, cportid, 0, hdr_size - sizeof(*hdr));
    if (!op)
        return;

    memcpy(op->request_buffer, data, hdr_size);
    unipro_unpause_rx(greybus->unipro, cportid);

    spinlock_lock(&cport->rx_fifo_lock);
    list_add(&cport->rx_fifo, &op->list);
    semaphore_unlock(&cport->rx_fifo_semaphore);
    spinlock_unlock(&cport->rx_fifo_lock);
}

static void gb_init_cport(struct gb_cport *cport)
{
    semaphore_init(&cport->rx_fifo_semaphore, 0);
    list_init(&cport->rx_fifo);
    list_init(&cport->tx_fifo);
    spinlock_init(&cport->rx_fifo_lock);
    spinlock_init(&cport->tx_fifo_lock);

    watchdog_init(&cport->timeout_wd);
    cport->timeout_wd.timeout = gb_operation_timeout;
    cport->timeout_wd.user_priv = cport;
    cport->timedout_operation.request_buffer = &timedout_hdr;
    list_init(&cport->timedout_operation.list);
}

int gb_register_driver(struct greybus *greybus, unsigned int cportid,
                       struct gb_driver *driver)
{
    struct gb_cport *cport;
    int retval;

    if (!driver)
        return -EINVAL;

    dev_info(&greybus->device, "registering driver on cport %u\n", cportid);

    cport = gb_get_cport(greybus, cportid);
    if (cport) {
        gb_error("driver already registered for CP%u\n", cportid);
        return -EEXIST;
    }

    if (!driver->op_handlers && driver->op_handlers_count > 0) {
        gb_error("Invalid driver\n");
        return -EINVAL;
    }

    if (driver->init) {
        retval = driver->init(cportid);
        if (retval) {
            gb_error("cannot init\n");
            return retval;
        }
    }

    if (driver->op_handlers) {
        qsort(driver->op_handlers, driver->op_handlers_count,
              sizeof(*driver->op_handlers), gb_compare_handlers);
    }

    cport = kzalloc(sizeof(*cport), MM_KERNEL);
    if (!cport)
        return -ENOMEM;

    gb_init_cport(cport);
    cport->bus = greybus;
    cport->id = cportid;
    cport->driver = driver;
    cport->thread = task_run(gb_pending_message_worker, cport, 0);
    if (!cport->thread)
        goto task_run_error;

    hashtable_add(greybus->cport_map, (void*) cportid, cport);
    dev_info(&greybus->device, "registered driver on cport %u\n", cportid);

    return 0;

task_run_error:
    if (driver->exit)
        driver->exit(cportid);
    kfree(cport);
    return retval;
}

int gb_listen(struct greybus *greybus, unsigned int cportid)
{
    struct gb_cport *cport = gb_get_cport(greybus, cportid);

    if (!cport) {
        gb_error("Invalid cport number %u\n", cportid);
        return -EINVAL;
    }

    if (!cport->driver) {
        gb_error("No driver registered! Can not connect CP%u.\n", cportid);
        return -EINVAL;
    }

    return unipro_register_cport_driver(greybus->unipro, cport->id,
                                        &greybus->unipro_cport_driver);
}

int gb_stop_listening(struct greybus *greybus, unsigned int cportid)
{
    struct gb_cport *cport = gb_get_cport(greybus, cportid);

    if (!cport) {
        gb_error("Invalid cport number %u\n", cportid);
        return -EINVAL;
    }

    if (!cport->driver) {
        gb_error("No driver registered! Can not connect CP%u.\n", cportid);
        return -EINVAL;
    }

    return unipro_unregister_cport_driver(greybus->unipro, cport->id);
}

static void gb_operation_timeout(struct watchdog *wdog)
{
    struct gb_cport *cport = wdog->user_priv;

    irq_disable();

    /* timedout operation could potentially already been queued */
    if (list_is_empty(&cport->timedout_operation.list)) {
        irq_enable();
        return;
    }

    list_add(&cport->rx_fifo, &cport->timedout_operation.list);
    semaphore_up(&cport->rx_fifo_semaphore);

    irq_enable();
}

int gb_operation_send_request(struct gb_operation *operation,
                              gb_operation_callback callback,
                              bool need_response)
{
    struct greybus *greybus = operation->greybus;
    struct gb_cport *cport = gb_get_cport(greybus, operation->cport);
    struct gb_operation_hdr *hdr = operation->request_buffer;
    ssize_t retval = 0;

    RET_IF_FAIL(operation, -EINVAL);

    hdr->id = 0;

    spinlock_lock(&cport->tx_fifo_lock);

    if (need_response) {
        hdr->id = cpu_to_le16(atomic_inc(&greybus->request_id));
        if (hdr->id == 0) /* ID 0 is for request with no response */
            hdr->id = cpu_to_le16(atomic_inc(&greybus->request_id));
        clock_gettime(CLOCK_MONOTONIC, &operation->time);
        operation->callback = callback;
        gb_operation_ref(operation);
        list_add(&cport->tx_fifo, &operation->list);

        if (!watchdog_is_active(&cport->timeout_wd))
            watchdog_start_msec(&cport->timeout_wd, TIMEOUT_IN_MS);
    }

    gb_dump(operation->request_buffer, hdr->size);
    retval = unipro_send(greybus->unipro, operation->cport,
                         operation->request_buffer, le16_to_cpu(hdr->size));
    if (need_response && retval < 0) {
        list_del(&operation->list);
        gb_watchdog_update(cport);
        gb_operation_unref(operation);
    }

    spinlock_unlock(&cport->tx_fifo_lock);

    return retval < 0 ? retval : 0;
}

static void gb_operation_callback_sync(struct gb_operation *operation)
{
    semaphore_unlock(&operation->sync_sem);
}

int gb_operation_send_request_sync(struct gb_operation *operation)
{
    int retval;

    semaphore_init(&operation->sync_sem, 0);

    retval =
        gb_operation_send_request(operation, gb_operation_callback_sync, true);
    if (retval)
        return retval;

    semaphore_lock(&operation->sync_sem);

    return 0;
}

static int gb_operation_send_oom_response(struct gb_operation *operation)
{
    int retval;
    struct gb_operation_hdr *req_hdr = operation->request_buffer;
    struct greybus *greybus = operation->greybus;

    irq_disable();

    oom_hdr.id = req_hdr->id;
    oom_hdr.type = TYPE_RESPONSE_FLAG | req_hdr->type;

    retval = unipro_send(greybus->unipro, operation->cport, &oom_hdr,
                         sizeof(oom_hdr));

    irq_enable();

    return retval;
}

int gb_operation_send_response(struct gb_operation *operation, uint8_t result)
{
    struct greybus *greybus = operation->greybus;
    struct gb_operation_hdr *resp_hdr;
    int retval;
    bool has_allocated_response = false;

    RET_IF_FAIL(operation, -EINVAL);

    if (operation->has_responded)
        return -EINVAL;

    if (!operation->response_buffer) {
        gb_operation_alloc_response(operation, 0);
        if (!operation->response_buffer)
            return gb_operation_send_oom_response(operation);

        has_allocated_response = true;
    }

    resp_hdr = operation->response_buffer;
    resp_hdr->result = result;

    gb_dump(operation->response_buffer, resp_hdr->size);
    retval = unipro_send(greybus->unipro, operation->cport,
                         operation->response_buffer,
                         le16_to_cpu(resp_hdr->size));
    if (retval < 0) {
        gb_error("Greybus backend failed to send: error %d\n", retval);
        if (has_allocated_response) {
            gb_debug("Free the response buffer\n");
            free(operation->response_buffer);
            operation->response_buffer = NULL;
        }
        return retval;
    }

    operation->has_responded = true;
    return 0;
}

void *gb_operation_alloc_response(struct gb_operation *operation, size_t size)
{
    struct gb_operation_hdr *req_hdr;
    struct gb_operation_hdr *resp_hdr;

    RET_IF_FAIL(operation, NULL);

    operation->response_buffer = malloc(size + sizeof(*resp_hdr));
    if (!operation->response_buffer) {
        gb_error("Can not allocate a response_buffer\n");
        return NULL;
    }

    memset(operation->response_buffer, 0, size + sizeof(*resp_hdr));

    req_hdr = operation->request_buffer;
    resp_hdr = operation->response_buffer;

    resp_hdr->size = cpu_to_le16(size + sizeof(*resp_hdr));
    resp_hdr->id = req_hdr->id;
    resp_hdr->type = TYPE_RESPONSE_FLAG | req_hdr->type;
    return gb_operation_get_response_payload(operation);
}

void gb_operation_destroy(struct gb_operation *operation)
{
    RET_IF_FAIL(operation,);
    gb_operation_unref(operation);
}

void gb_operation_ref(struct gb_operation *operation)
{
    RET_IF_FAIL(operation,);
    RET_IF_FAIL(atomic_get(&operation->ref_count) > 0,);
    atomic_inc(&operation->ref_count);
}

void gb_operation_unref(struct gb_operation *operation)
{
    RET_IF_FAIL(operation,);
    RET_IF_FAIL(atomic_get(&operation->ref_count) > 0,);

    uint32_t ref_count = atomic_dec(&operation->ref_count);
    if (ref_count != 0) {
        return;
    }

    free(operation->request_buffer);
    free(operation->response_buffer);
    if (operation->response) {
        gb_operation_unref(operation->response);
    }
    free(operation);
}


struct gb_operation *gb_operation_create(struct greybus *greybus,
                                         unsigned int cportid, uint8_t type,
                                         uint32_t req_size)
{
    struct gb_cport *cport = gb_get_cport(greybus, cportid);
    struct gb_operation *operation;
    struct gb_operation_hdr *hdr;

    if (!cport)
        return NULL;

    operation = malloc(sizeof(*operation));
    if (!operation)
        return NULL;

    memset(operation, 0, sizeof(*operation));
    operation->cport = cportid;
    operation->greybus = greybus;

    operation->request_buffer = malloc(req_size + sizeof(*hdr));
    if (!operation->request_buffer)
        goto malloc_error;

    memset(operation->request_buffer, 0, req_size + sizeof(*hdr));
    hdr = operation->request_buffer;
    hdr->size = cpu_to_le16(req_size + sizeof(*hdr));
    hdr->type = type;

    list_init(&operation->list);
    atomic_init(&operation->ref_count, 1);

    return operation;
malloc_error:
    free(operation);
    return NULL;
}

size_t gb_operation_get_request_payload_size(struct gb_operation *operation)
{
    struct gb_operation_hdr *hdr;

    if (!operation || !operation->request_buffer) {
        return 0;
    }

    hdr = operation->request_buffer;
    if (le16_to_cpu(hdr->size) < sizeof(*hdr)) {
        return 0;
    }

    return le16_to_cpu(hdr->size) - sizeof(*hdr);
}

uint8_t gb_operation_get_request_result(struct gb_operation *operation)
{
    struct gb_operation_hdr *hdr;

    if (!operation) {
        return GB_OP_INTERNAL;
    }

    if (!operation->response) {
        return GB_OP_TIMEOUT;
    }

    hdr = operation->response->request_buffer;
    if (!hdr || hdr->size < sizeof(*hdr)) {
        return GB_OP_INTERNAL;
    }

    return hdr->result;
}

int gb_tape_register_mechanism(struct greybus *greybus,
                               struct gb_tape_mechanism *mechanism)
{
    if (!greybus)
        return -EINVAL;

    if (!mechanism || !mechanism->open || !mechanism->close ||
        !mechanism->read || !mechanism->write)
        return -EINVAL;

    if (greybus->tape)
        return -EBUSY;

    greybus->tape = mechanism;

    return 0;
}

int gb_tape_communication(struct greybus *greybus, const char *pathname)
{
    if (!greybus || !greybus->tape)
        return -EINVAL;

    if (greybus->tape_fd >= 0)
        return -EBUSY;

    greybus->tape_fd = greybus->tape->open(pathname, GB_TAPE_WRONLY);
    if (greybus->tape_fd < 0)
        return greybus->tape_fd;

    return 0;
}

int gb_tape_stop(struct greybus *greybus)
{
    if (!greybus || !greybus->tape || greybus->tape_fd < 0)
        return -EINVAL;

    greybus->tape->close(greybus->tape_fd);
    greybus->tape_fd = -EBADF;

    return 0;
}

int gb_tape_replay(struct greybus *greybus, const char *pathname)
{
    struct unipro_cport_driver *cport_drv = &greybus->unipro_cport_driver;
    struct gb_tape_record_header hdr;
    char *buffer;
    ssize_t nread;
    int retval = 0;
    int fd;

    if (!greybus || !pathname || !greybus->tape)
        return -EINVAL;

    dev_info(&greybus->device, "greybus: replaying '%s'...\n", pathname);

    fd = greybus->tape->open(pathname, GB_TAPE_RDONLY);
    if (fd < 0)
        return fd;

    buffer = kzalloc(GREYBUS_MTU, MM_KERNEL);
    if (!buffer) {
        retval = -ENOMEM;
        goto error_buffer_alloc;
    }

    while (1) {
        nread = greybus->tape->read(fd, &hdr, sizeof(hdr));
        if (!nread)
            break;

        if (nread != sizeof(hdr)) {
            gb_error("gb-tape: invalid byte count read, aborting...\n");
            retval = -EIO;
            break;
        }

        nread = greybus->tape->read(fd, buffer, hdr.size);
        if (hdr.size != nread) {
            gb_error("gb-tape: invalid byte count read, aborting...\n");
            retval = -EIO;
            break;
        }

        greybus_rx_handler(cport_drv, hdr.cport, buffer, nread);
    }

    free(buffer);

error_buffer_alloc:
    greybus->tape->close(fd);

    return retval;
}

static void *gb_get_buffer(void)
{
    return page_alloc(MM_DMA, size_to_order(GREYBUS_MTU / PAGE_SIZE));
}

static int gb_probe(struct device *device)
{
    struct greybus *greybus = containerof(device, struct greybus, device);

    greybus->cport_map = hashtable_create_uint();
    atomic_init(&greybus->request_id, 0);
    greybus->tape_fd = -EBADF;

    greybus->unipro_cport_driver.get_buffer = gb_get_buffer,
    greybus->unipro_cport_driver.rx_handler = greybus_rx_handler;

    return 0;
}

__driver__ struct driver gb_unipro_driver = {
    .name = "greybus",
    .probe = gb_probe,
};
