#ifndef __GREYBUS_AP_SVC_H__
#define __GREYBUS_AP_SVC_H__

int gb_svc_assign_device_id(struct gb_interface *iface);
int gb_svc_create_route(struct gb_interface *iface);
int gb_svc_create_connection(struct gb_connection *connection);
int gb_svc_destroy_connection(struct gb_connection *connection);

#endif /* __GREYBUS_AP_SVC_H__ */

