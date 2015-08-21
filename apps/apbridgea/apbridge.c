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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <phabos/usb/gadget/gb-ap-bridge.h>
#include <phabos/greybus.h>
#include <phabos/greybus/debug.h>
#include <phabos/scheduler.h>
#include <phabos/unipro.h>
#include <phabos/unipro/tsb.h>
#include <apps/shell.h>

#include "apbridge_backend.h"
#include "utils.h"

static struct apbridge_dev_s *g_usbdev = NULL;
static struct task *g_svc_thread;
static struct apbridge_backend apbridge_backend;

static int release_buffer(int status, const void *buf, void *priv)
{
    return usb_release_buffer(priv, buf);
}

static int usb_to_unipro(struct apbridge_dev_s *dev, void *buf, size_t len)
{
    struct gb_operation_hdr *hdr = buf;
    unsigned int cportid;

    gb_dump(buf, len);

    if (len < sizeof(*hdr))
        return -EPROTO;

    /*
     * Retreive and clear the cport id stored in the header pad bytes.
     */
    cportid = hdr->pad[1] << 8 | hdr->pad[0];
    hdr->pad[0] = 0;
    hdr->pad[1] = 0;

    return apbridge_backend.usb_to_unipro(cportid, buf, len,
                                          release_buffer, dev);
}

void recv_from_unipro(struct unipro_cport_driver *cport_drv,
                      unsigned int cportid, void *buf, size_t len)
{
    struct gb_operation_hdr *hdr = (void *)buf;

    /*
     * FIXME: Remove when UniPro driver provides the actual buffer length.
     */
    len = gb_packet_size(buf);

    gb_dump(buf, len);

    /* Store the cport id in the header pad bytes (if we have a header). */
    if (len >= sizeof(*hdr)) {
        hdr->pad[0] = cportid & 0xff;
        hdr->pad[1] = (cportid >> 8) & 0xff;
    }

    unipro_to_usb(g_usbdev, buf, len);
}

static void svc_sim_fn(void *p_data)
{
    struct apbridge_dev_s *priv = p_data;

    usb_wait(priv);
    apbridge_backend.init();
    /*
     * Tell the SVC that the AP Module is ready
     */
    extern struct unipro_device tsb_unipro;
    tsb_unipro_mbox_set(&tsb_unipro, TSB_MAIL_READY_AP, true);
}

static int svc_sim_init(struct apbridge_dev_s *priv)
{
    g_usbdev = priv;
    g_svc_thread = task_run(svc_sim_fn, priv, 0);
    return g_svc_thread ? 0 : -1;
}

static struct apbridge_usb_driver usb_driver = {
    .usb_to_unipro = usb_to_unipro,
    .init = svc_sim_init,
};

int bridge_main(int argc, char *argv[])
{
    up_usbinitialize();
    apbridge_backend_register(&apbridge_backend);
    usbdev_apbinitialize(&usb_driver);

    shell_main(argc, argv);

    return 0;
}
