#ifndef __GREYBUS_UNIPRO_H__
#define __GREYBUS_UNIPRO_H__

struct gb_unipro {
    struct greybus bus;
    struct unipro_device *dev;
};

#endif /* __GREYBUS_UNIPRO_H__ */

