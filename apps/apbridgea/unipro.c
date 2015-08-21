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

#include <phabos/unipro.h>
#include <phabos/greybus.h>
#include <phabos/mm.h>

#include "apbridge_backend.h"

/*
 * TODO
 * Already defined in tsb_unipro.c
 * Move them to tsb_unipro.h
 */

#define CPORTID_CDSI0    (16)
#define CPORTID_CDSI1    (17)

extern struct unipro_device tsb_unipro;

static int unipro_usb_to_unipro(unsigned int cportid, void *buf, size_t len,
                                unipro_send_completion_t callback, void *priv)
{
    return unipro_send_async(&tsb_unipro, cportid, buf, len, callback, priv);
}

static int unipro_usb_to_svc(void *buf, size_t len)
{
    return svc_handle(buf, len);
}

static void *unipro_get_buffer(void)
{
    return page_alloc(MM_DMA, size_to_order(GREYBUS_MTU / PAGE_SIZE));
}

static struct unipro_cport_driver unipro_driver = {
    .get_buffer = unipro_get_buffer,
    .rx_handler = recv_from_unipro,
};

static void unipro_backend_init(void)
{
    /* Now register a driver for those CPorts */
    for (int i = 0; i < tsb_unipro.cport_count; i++) {
        /* These cports are already allocated for display and camera */
        if (i == CPORTID_CDSI0 || i == CPORTID_CDSI1)
            continue;
        unipro_register_cport_driver(&tsb_unipro, i, &unipro_driver);
    }
}

void apbridge_backend_register(struct apbridge_backend *apbridge_backend)
{
    apbridge_backend->usb_to_unipro = unipro_usb_to_unipro;
    apbridge_backend->usb_to_svc = unipro_usb_to_svc;
    apbridge_backend->init = unipro_backend_init;
}
