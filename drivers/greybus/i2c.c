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

#include <phabos/i2c.h>
#include <phabos/greybus.h>

#include "i2c-gb.h"

#define GB_I2C_VERSION_MAJOR 0
#define GB_I2C_VERSION_MINOR 1

#define I2C_FUNC_I2C                    0x00000001
#define I2C_FUNC_SMBUS_READ_BYTE        0x00020000
#define I2C_FUNC_SMBUS_WRITE_BYTE       0x00040000
#define I2C_M_RD                        0x0001

/* TODO move it inside a struct */
struct i2c_dev *i2c_dev;

static uint8_t gb_i2c_protocol_version(struct gb_operation *operation)
{
    struct gb_i2c_proto_version_response *response;

    response = gb_operation_alloc_response(operation, sizeof(*response));
    if (!response)
        return GB_OP_NO_MEMORY;

    response->major = GB_I2C_VERSION_MAJOR;
    response->minor = GB_I2C_VERSION_MINOR;
    return GB_OP_SUCCESS;
}

static uint8_t gb_i2c_protocol_functionality(struct gb_operation *operation)
{
    struct gb_i2c_functionality_rsp *response;

    response = gb_operation_alloc_response(operation, sizeof(*response));
    if (!response)
        return GB_OP_NO_MEMORY;

    response->functionality = I2C_FUNC_I2C |
                              I2C_FUNC_SMBUS_READ_BYTE |
                              I2C_FUNC_SMBUS_WRITE_BYTE;

    return GB_OP_SUCCESS;
}

static uint8_t gb_i2c_protocol_timeout(struct gb_operation *operation)
{
    return GB_OP_SUCCESS;
}

static uint8_t gb_i2c_protocol_retries(struct gb_operation *operation)
{
    return GB_OP_SUCCESS;
}

static uint8_t gb_i2c_protocol_transfer(struct gb_operation *operation)
{
    int i, op_count;
    uint32_t size = 0;
    int ret;
    uint8_t *write_data;
    bool read_op;
    int read_count = 0;
    struct i2c_msg *msg;
    struct gb_i2c_transfer_desc *desc;
    struct gb_i2c_transfer_req *request;
    struct gb_i2c_transfer_rsp *response;

    request = (struct gb_i2c_transfer_req *)
                  gb_operation_get_request_payload(operation);
    op_count = request->op_count;
    write_data = (uint8_t *)&request->desc[op_count];

    for (i = 0; i < op_count; i++) {
        desc = &request->desc[i];
        read_op = (desc->flags & I2C_M_RD) ? true : false;

        if (read_op)
            size += desc->size;
    }

    msg = malloc(sizeof(struct i2c_msg) * op_count);
    if (!msg)
        return GB_OP_NO_MEMORY;
    response = gb_operation_alloc_response(operation, size);
    if (!response) {
           ret = GB_OP_NO_MEMORY;
           goto err_free_msg;
    }

    for (i = 0; i < op_count; i++) {
        desc = &request->desc[i];
        read_op = (desc->flags & I2C_M_RD) ? true : false;

        msg[i].flags = 0;
        msg[i].addr = desc->addr;
        if (read_op) {
            msg[i].flags |= I2C_READ;
            msg[i].buffer = &response->data[read_count];
            read_count += desc->size;
        } else {
            msg[i].buffer = write_data;
            write_data += desc->size;
        }
        msg[i].length = desc->size;
    }
    ret = i2c_transfer(i2c_dev, msg, op_count);
    free(msg);
    if (ret == -EIO)
      return GB_OP_NONEXISTENT;
    else if (ret)
      return GB_OP_UNKNOWN_ERROR;

    return GB_OP_SUCCESS;

err_free_msg:
    free(msg);

    return ret;
}

static int gb_i2c_init(unsigned int cport)
{
    i2c_dev = i2c_initialize(0);
    return 0;
}

static struct gb_operation_handler gb_i2c_handlers[] = {
    GB_HANDLER(GB_I2C_PROTOCOL_VERSION, gb_i2c_protocol_version),
    GB_HANDLER(GB_I2C_PROTOCOL_FUNCTIONALITY, gb_i2c_protocol_functionality),
    GB_HANDLER(GB_I2C_PROTOCOL_TIMEOUT, gb_i2c_protocol_timeout),
    GB_HANDLER(GB_I2C_PROTOCOL_RETRIES, gb_i2c_protocol_retries),
    GB_HANDLER(GB_I2C_PROTOCOL_TRANSFER, gb_i2c_protocol_transfer),
};

static struct gb_driver gb_i2c_driver = {
    .init = gb_i2c_init,
    .op_handlers = gb_i2c_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_i2c_handlers),
};

void gb_i2c_register(int cport)
{
    gb_register_driver(cport, &gb_i2c_driver);
}
