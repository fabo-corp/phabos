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

#include <errno.h>
#include <asm/delay.h>
#include <asm/unipro.h>
#include <phabos/greybus.h>

static int gb_unipro_rx_handler(unsigned int cport, void *data, size_t size)
{
    int retval;

    retval = greybus_rx_handler(cport, data, size);
    unipro_unpause_rx(cport);

    return retval;
}

static struct unipro_driver greybus_driver = {
    .name = "greybus",
    .rx_handler = gb_unipro_rx_handler,
};

static int gb_unipro_listen(unsigned int cport)
{
#if 1
    int ret;

    gb_debug("Connecting cport %d\n", cport);
    do {
        ret = unipro_init_cport(cport);
        if (!ret)
            ret = unipro_driver_register(&greybus_driver, cport);
        else
            mdelay(200);
        kprintf("loop\n");
    } while (ret == -ENOTCONN);
    return ret;
#else
    return unipro_driver_register(&greybus_driver, cport);
#endif
}

static int gb_unipro_stop_listening(unsigned int cport)
{
    return unipro_driver_unregister(cport);
}

static struct gb_transport_backend gb_unipro_backend = {
    .init = unipro_init,
    .send = unipro_send,
    .listen = gb_unipro_listen,
    .stop_listening = gb_unipro_stop_listening,
};

static int gb_unipro_init(struct driver *driver)
{
    gb_debug("Greybus: register unipro backend\n");
    return gb_init(&gb_unipro_backend);
}

__driver__ struct driver gb_unipro_driver = {
    .name = "gb-unipro",
    .init = gb_unipro_init,
};
