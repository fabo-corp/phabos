#include <phabos/greybus-types.h>

/* Version of the Greybus SVC protocol we support */
#define GB_SVC_VERSION_MAJOR		0x00
#define GB_SVC_VERSION_MINOR		0x01

/* Greybus SVC request types */
#define GB_SVC_TYPE_INVALID		0x00
#define GB_SVC_TYPE_PROTOCOL_VERSION	0x01
#define GB_SVC_TYPE_SVC_HELLO		0x02
#define GB_SVC_TYPE_INTF_DEVICE_ID	0x03
#define GB_SVC_TYPE_INTF_HOTPLUG	0x04
#define GB_SVC_TYPE_INTF_HOT_UNPLUG	0x05
#define GB_SVC_TYPE_INTF_RESET		0x06
#define GB_SVC_TYPE_CONN_CREATE		0x07
#define GB_SVC_TYPE_CONN_DESTROY	0x08

/* SVC version request/response have same payload as gb_protocol_version_response */

/* SVC protocol hello request */
struct gb_svc_hello_request {
	__le16			endo_id;
	__u8			interface_id;
};
/* hello response has no payload */

struct gb_svc_intf_device_id_request {
	__u8	intf_id;
	__u8	device_id;
};
/* device id response has no payload */

struct gb_svc_intf_hotplug_request {
	__u8	intf_id;
	struct {
		__le32	unipro_mfg_id;
		__le32	unipro_prod_id;
		__le32	ara_vend_id;
		__le32	ara_prod_id;
	} data;
};
/* hotplug response has no payload */

struct gb_svc_intf_hot_unplug_request {
	__u8	intf_id;
};
/* hot unplug response has no payload */

struct gb_svc_intf_reset_request {
	__u8	intf_id;
};
/* interface reset response has no payload */

struct gb_svc_conn_create_request {
	__u8	intf1_id;
	__le16	cport1_id;
	__u8	intf2_id;
	__le16	cport2_id;
};
/* connection create response has no payload */

struct gb_svc_conn_destroy_request {
	__u8	intf1_id;
	__le16	cport1_id;
	__u8	intf2_id;
	__le16	cport2_id;
};
/* connection destroy response has no payload */
