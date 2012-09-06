#include <libusb-0.1.12/usb.h>
int usb_device_scsi_inquiry (struct usb_dev_handle *usb_device_handle,int interface);
int get_usb_device_configuration(struct usb_dev_handle* device_handle);
int get_usb_switch_configuration_index(int vendor,int product);
struct usb_device* get_usb_device(int vendor,int product);


// internal usb_modeswitch configurations - this can be overwritten by setting
// ro.dongled.external_config = 1
struct usb_modeswitch_config {
	int vendor;
	int product;
	char *message;
} usbmsconfigs[] = { 0x12d1,0x1c0b,"55534243123456780000000000000011062000000100000000000000000000",
	0x1bbb,0xf000,"55534243123456788000000080000606f50402527000000000000000000000" };