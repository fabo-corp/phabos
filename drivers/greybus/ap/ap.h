#ifndef __GREYBUS_AP_H__
#define __GREYBUS_AP_H__

struct gb_device;
struct greybus;

struct gb_device *gb_ap_create_device(struct greybus *bus, unsigned cport);

#endif /* __GREYBUS_AP_H__ */

