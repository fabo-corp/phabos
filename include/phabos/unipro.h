/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __PHABOS_UNIPRO_H__
#define __PHABOS_UNIPRO_H__

#include <stdint.h>
#include <phabos/driver.h>

#define CPORT_BUF_SIZE              (1024)

struct unipro_device;
struct unipro_cport;

typedef int (*unipro_send_completion_t)(int status, const void *buf,
                                        void *priv);

struct unipro_cport_ops {
    ssize_t (*send)(struct unipro_cport *cport, const void *buffer, size_t len);
    ssize_t (*send_async)(struct unipro_cport *cport, const void *buf,
                          size_t len, unipro_send_completion_t callback,
                          void *user_priv);

    int (*unpause_rx)(struct unipro_cport *cport);
    int (*init)(struct unipro_cport *cport);
};

struct unipro_ops {
    int (*local_attr_read)(struct unipro_device *device, uint16_t attr,
                           uint32_t *val, uint16_t selector,
                           uint32_t *result_code);
    int (*local_attr_write)(struct unipro_device *device, uint16_t attr,
                            uint32_t val, uint16_t selector,
                            uint32_t *result_code);
    int (*peer_attr_read)(struct unipro_device *device, uint16_t attr,
                          uint32_t *val, uint16_t selector,
                          uint32_t *result_code);
    int (*peer_attr_write)(struct unipro_device *device, uint16_t attr,
                           uint32_t val, uint16_t selector,
                           uint32_t *result_code);

    struct unipro_cport_ops cport;
};

struct unipro_cport_driver {
    void *(*get_buffer)(void);
    void (*rx_handler)(struct unipro_cport_driver *cport_driver, unsigned cport,
                       void *buffer, size_t len);
};

struct unipro_cport {
    struct unipro_device *device;

    unsigned id;
    bool is_connected;

    struct unipro_cport_driver *driver;
    struct list_head tx_fifo;
};

struct unipro_device {
    struct device device;

    struct unipro_cport *cports;
    struct unipro_ops *ops;
    size_t cport_count;
};

int unipro_register_device(struct unipro_device *device,
                           struct unipro_ops *ops);
int unipro_register_cport_driver(struct unipro_device *device, unsigned cport,
                                 struct unipro_cport_driver *driver);
int unipro_unregister_cport_driver(struct unipro_device *device,
                                   unsigned cport);

static inline struct unipro_device *to_unipro_device(struct device *device)
{
    return containerof(device, struct unipro_device, device);
}

static inline int unipro_attr_local_read(struct unipro_device *device,
                                         uint16_t attr, uint32_t *val,
                                         uint16_t selector,
                                         uint32_t *result_code)
{
    return device->ops->local_attr_read(device, attr, val, selector,
                                        result_code);
}

static inline int unipro_attr_peer_read(struct unipro_device *device,
                                        uint16_t attr, uint32_t *val,
                                        uint16_t selector,
                                        uint32_t *result_code)
{
    return device->ops->peer_attr_read(device, attr, val, selector,
                                       result_code);
}

static inline int unipro_attr_local_write(struct unipro_device *device,
                                          uint16_t attr, uint32_t val,
                                          uint16_t selector,
                                          uint32_t *result_code)
{
    return device->ops->local_attr_write(device, attr, val, selector,
                                         result_code);
}

static inline int unipro_attr_peer_write(struct unipro_device *device,
                                         uint16_t attr, uint32_t val,
                                         uint16_t selector,
                                         uint32_t *result_code)
{
    return device->ops->peer_attr_write(device, attr, val, selector,
                                        result_code);
}

static inline ssize_t unipro_send(struct unipro_device *device,
                                  unsigned cportid, const void *buffer,
                                  size_t len)
{
    return device->ops->cport.send(&device->cports[cportid], buffer, len);
}

static inline ssize_t unipro_send_async(struct unipro_device *device,
                                        unsigned cportid, const void *buf,
                                        size_t len,
                                        unipro_send_completion_t callback,
                                        void *user_priv)
{
    return device->ops->cport.send_async(&device->cports[cportid], buf, len,
                                         callback, user_priv);
}

static inline int unipro_unpause_rx(struct unipro_device *device,
                                    unsigned cportid)
{
    return device->ops->cport.unpause_rx(&device->cports[cportid]);
}

#endif /* __PHABOS_UNIPRO_H__ */

