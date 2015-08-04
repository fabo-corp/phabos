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
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <phabos/list.h>
#include <phabos/utils.h>
#include <phabos/scheduler.h>
#include <phabos/greybus.h>
#include <phabos/greybus/loopback.h>
#include "loopback-gb.h"
#include <asm/byteordering.h>

#define GB_LOOPBACK_VERSION_MAJOR 0
#define GB_LOOPBACK_VERSION_MINOR 1

struct gb_loopback {
    struct list_head list;
    int ms;
    int type;
    int enomem;
    size_t size;
    unsigned int error;
    unsigned int cportid;
};

LIST_DECLARE(gb_loopbacks);

void gb_loopback_fn(void *data)
{
    int ms;
    int type;
    size_t size;
    struct gb_loopback *gb_loopback = data;

    while(1) {
        if (gb_loopback->type == 0) {
            sleep(1);
            continue;
        }

        /* mutex lock */
        ms = gb_loopback->ms;
        type = gb_loopback->type;
        size = gb_loopback->size;
        /* mutex unlock */

        if (type == 1) {
            if (gb_loopback_send_req(gb_loopback, 1,
                                     GB_LOOPBACK_TYPE_PING)) {
                gb_loopback->enomem++;
            }
        }
        if (type == 2) {
            if (gb_loopback_send_req(gb_loopback, size,
                                     GB_LOOPBACK_TYPE_TRANSFER)) {
                gb_loopback->enomem++;
            }
        }
        if (type == 3) {
            if (gb_loopback_send_req(gb_loopback, size,
                                     GB_LOOPBACK_TYPE_SINK)) {
                gb_loopback->enomem++;
            }
        }
        if (ms) {
            usleep(ms * 1000);
        }
    }
}

struct gb_loopback *list_to_loopback(struct list_head *iter)
{
    return list_entry(iter, struct gb_loopback, list);
}

struct gb_loopback *cport_to_loopback(unsigned int cportid)
{
    struct gb_loopback *gb_loopback;

    list_foreach(&gb_loopbacks, iter) {
        gb_loopback = list_entry(iter, struct gb_loopback, list);
        if (gb_loopback->cportid == cportid)
            return gb_loopback;
    }

    return NULL;
}

unsigned int gb_loopback_to_cport(struct gb_loopback *gb_loopback)
{
    return gb_loopback->cportid;
}

static int gb_loopback_reset(struct gb_loopback *gb_loopback)
{
    if (!gb_loopback) {
        return -EINVAL;
    }

    gb_loopback->error = 0;
    gb_loopback->enomem = 0;

    return 0;
}

int gb_loopback_cport_conf(struct gb_loopback *gb_loopback,
                           int type, size_t size, int ms)
{
    gb_loopback_reset(gb_loopback);
    /* mutex lock*/
    if (gb_loopback == NULL)
        return 1;
    gb_loopback->type = type;
    gb_loopback->size = size;
    gb_loopback->ms = ms;
    /* mutex unlock */
    return 0;
}

static uint8_t gb_loopback_protocol_version(struct gb_operation *operation)
{
    struct gb_loopback_proto_version_response *response;

    response = gb_operation_alloc_response(operation, sizeof(*response));
    if(!response)
        return GB_OP_NO_MEMORY;

    response->major = GB_LOOPBACK_VERSION_MAJOR;
    response->minor = GB_LOOPBACK_VERSION_MINOR;
    return GB_OP_SUCCESS;
}

static uint8_t gb_loopback_transfer(struct gb_operation *operation)
{
    struct gb_loopback_transfer_response *response;
    struct gb_loopback_transfer_request *request =
        gb_operation_get_request_payload(operation);
    size_t request_length = le32_to_cpu(request->len);

    response = gb_operation_alloc_response(operation,
                                           sizeof(*response) + request_length);
    if(!response)
        return GB_OP_NO_MEMORY;
    memcpy(response->data, request->data, request_length);
    return GB_OP_SUCCESS;
}

static uint8_t gb_loopback_ping(struct gb_operation *operation)
{
    return GB_OP_SUCCESS;
}

/**
 * @brief           Called upon reception of a 'sink' operation request
 *                  (receiving end)
 * @param[in]       operation: greybus loopback operation
 */
static uint8_t gb_loopback_sink_req_cb(struct gb_operation *operation)
{
    /* Ignore data payload, just acknowledge the operation. */
    return GB_OP_SUCCESS;
}

static void gb_loopback_transfer_sync(struct gb_operation *operation)
{
    struct gb_loopback *gb_loopback;
    struct gb_loopback_transfer_response *response;
    struct gb_loopback_transfer_request *request;

    request = gb_operation_get_request_payload(operation);
    response = gb_operation_get_request_payload(operation->response);

    gb_loopback = cport_to_loopback(operation->cport);
    if (!gb_loopback) {
        return;
    }

    if (memcmp(request->data, response->data, le32_to_cpu(request->len)))
        gb_loopback->error += 1;
}

/**
 * @brief           called upon reception of a 'sink' operation response
 *                  (sending end)
 * @param[in]       operation: received greybus loopback operation
 */
static void gb_loopback_sink_resp_cb(struct gb_operation *operation)
{
    /*
     * FIXME: operation result shall be verified, but bug #826 implementing
     * this feature is still under development/review. To be completed.
     */
}

/**
 * @brief           Send loopback operation request
 * @return          OK in case of success, <0 otherwise
 * @param[in]       gb_loopback: gb_loopback private driver data
 * @param[in]       size: request payload size in bytes
 * @param[in]       type: operation type (ping / transfer / sink)
 */
int gb_loopback_send_req(struct gb_loopback *gb_loopback,
                         size_t size, uint8_t type)
{
    int i;
    struct gb_operation *operation;
    struct gb_loopback_transfer_request *request;

    if (!gb_loopback) {
        return -EINVAL;
    }

    switch(type) {
    case GB_LOOPBACK_TYPE_PING:
        operation = gb_operation_create(gb_loopback->cportid,
                                        GB_LOOPBACK_TYPE_PING, 1);
        break;
    case GB_LOOPBACK_TYPE_TRANSFER:
    case GB_LOOPBACK_TYPE_SINK:
        operation = gb_operation_create(gb_loopback->cportid,
                                        type,
                                        sizeof(*request) + size);
        break;
    default:
        return -EINVAL;

    }
    if (!operation)
        return -ENOMEM;

    switch(type) {
    case GB_LOOPBACK_TYPE_PING:
        gb_operation_send_request_sync(operation);
        break;
    case GB_LOOPBACK_TYPE_TRANSFER:
    case GB_LOOPBACK_TYPE_SINK:
        request = gb_operation_get_request_payload(operation);
        request->len = cpu_to_le32(size);
        if (type == GB_LOOPBACK_TYPE_TRANSFER) {
            for (i = 0; i < size; i++) {
                request->data[i] = rand() & 0xFF;
            }
            gb_operation_send_request(operation,
                                      gb_loopback_transfer_sync, true);
        } else {
            /*
             * Data payload is ignored on receiver end.
             * No need to fill the buffer with some data.
             */
            gb_operation_send_request(operation,
                                      gb_loopback_sink_resp_cb, true);
        }
        break;
    default:
        return -EINVAL;

    }

    gb_operation_destroy(operation);
    return 0;
}

int gb_loopback_status(struct gb_loopback *gb_loopback)
{
    if (!gb_loopback) {
        return -EINVAL;
    }

    return gb_loopback->error + gb_loopback->enomem;
}

static struct gb_operation_handler gb_loopback_handlers[] = {
    GB_HANDLER(GB_LOOPBACK_TYPE_PROTOCOL_VERSION,
               gb_loopback_protocol_version),
    GB_HANDLER(GB_LOOPBACK_TYPE_PING, gb_loopback_ping),
    GB_HANDLER(GB_LOOPBACK_TYPE_TRANSFER, gb_loopback_transfer),
    GB_HANDLER(GB_LOOPBACK_TYPE_SINK, gb_loopback_sink_req_cb),
};

struct gb_driver loopback_driver = {
    .op_handlers = (struct gb_operation_handler *)gb_loopback_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_loopback_handlers),
};

void gb_loopback_register(int cport)
{
    struct gb_loopback *gb_loopback = zalloc(sizeof(*gb_loopback));
    if (gb_loopback) {
        gb_loopback->cportid = cport;
        list_add(&gb_loopbacks, &gb_loopback->list);
        task_run(gb_loopback_fn, gb_loopback, 0);
    }
    gb_register_driver(cport, &loopback_driver);
}
