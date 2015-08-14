/*
 * Copyright (c) 2015 Google Inc.
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
 * Author: Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <string.h>
#include <errno.h>

#include <asm/byteordering.h>

#include <phabos/assert.h>
#include <phabos/greybus.h>
#include <phabos/greybus/debug.h>

#include "control-gb.h"

#define CONTROL_CPORT 0

static struct gb_manifest *manifest;

static uint8_t gb_control_protocol_version(struct gb_operation *operation)
{
    struct gb_control_proto_version_response *response;

    kprintf("%s()\n", __func__);

    response = gb_operation_alloc_response(operation, sizeof(*response));
    if (!response)
        return GB_OP_NO_MEMORY;

    response->major = GB_CONTROL_VERSION_MAJOR;
    response->minor = GB_CONTROL_VERSION_MINOR;
    return GB_OP_SUCCESS;
}

/* Only the AP need to send authentication data in reply to this */
static uint8_t gb_control_probe_ap(struct gb_operation *operation)
{
    return GB_OP_SUCCESS;
}

static uint8_t gb_control_get_manifest_size(struct gb_operation *operation)
{
    struct gb_control_get_manifest_size_response *response;

    kprintf("%s()\n", __func__);

    response = gb_operation_alloc_response(operation, sizeof(*response));
    if (!response)
        return GB_OP_NO_MEMORY;

    response->size = cpu_to_le16(manifest->size);

    return GB_OP_SUCCESS;
}

static uint8_t gb_control_get_manifest(struct gb_operation *operation)
{
    struct gb_control_get_manifest_response *response;

    kprintf("%s()\n", __func__);

    response = gb_operation_alloc_response(operation, manifest->size);
    if (!response)
        return GB_OP_NO_MEMORY;

    memcpy(response->data, manifest->data, manifest->size);

    return GB_OP_SUCCESS;
}

static uint8_t gb_control_connected(struct gb_operation *operation)
{
    int retval;
    struct gb_control_connected_request *request =
        gb_operation_get_request_payload(operation);

    kprintf("%s()\n", __func__);

    retval = gb_listen(le16_to_cpu(request->cport_id));
    if (retval) {
        gb_error("Can not connect cport %d: error %d\n",
                 le16_to_cpu(request->cport_id), retval);
        return GB_OP_INVALID;
    }

    return GB_OP_SUCCESS;
}

static uint8_t gb_control_disconnected(struct gb_operation *operation)
{
    int retval;
    struct gb_control_connected_request *request =
        gb_operation_get_request_payload(operation);

    kprintf("%s()\n", __func__);

    retval = gb_stop_listening(le16_to_cpu(request->cport_id));
    if (retval) {
        gb_error("Can not disconnect cport %d: error %d\n",
                 le16_to_cpu(request->cport_id), retval);
        return GB_OP_INVALID;
    }

    return GB_OP_SUCCESS;
}

static struct gb_operation_handler gb_control_handlers[] = {
    GB_HANDLER(GB_CONTROL_TYPE_PROTOCOL_VERSION, gb_control_protocol_version),
    GB_HANDLER(GB_CONTROL_TYPE_PROBE_AP, gb_control_probe_ap),
    GB_HANDLER(GB_CONTROL_TYPE_GET_MANIFEST_SIZE, gb_control_get_manifest_size),
    GB_HANDLER(GB_CONTROL_TYPE_GET_MANIFEST, gb_control_get_manifest),
    GB_HANDLER(GB_CONTROL_TYPE_CONNECTED, gb_control_connected),
    GB_HANDLER(GB_CONTROL_TYPE_DISCONNECTED, gb_control_disconnected),
};

static struct gb_driver control_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_control_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_control_handlers),
};

static int gb_control_probe(struct device *device)
{
    int retval;

    RET_IF_FAIL(device, -EINVAL);
    RET_IF_FAIL(device->priv, -EINVAL);

    manifest = device->priv;

    retval = gb_register_driver(CONTROL_CPORT, &control_driver);
    if (retval)
        return retval;

    return gb_listen(CONTROL_CPORT);
}

__driver__ struct driver gb_control_driver = {
    .name = "gb-control-gpb",
    .probe = gb_control_probe,
};
