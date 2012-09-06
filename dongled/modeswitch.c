/*
 * 
 * 	modeswitch.c - Internal implementation of usb_modeswitch functionality
 * 	a streamlined implementation of the functionality provided usb_modeswitch
 * 	we don't care about target device id's or numerous other configuration parameters.
 * 	implementing the functionality here reduces the external failure points.
 * 	
 */
#define LOG_TAG "dongled"
#include <cutils/log.h>
#include "modeswitch.h"
#define DESCR_MAX 129
#define USB_DIR_OUT 0x00
#define USB_DIR_IN  0x80
#define BUF_SIZE 4096
/* Print result of SCSI command INQUIRY (identification details) */
int usb_device_scsi_inquiry (struct usb_dev_handle *usb_device_handle,int interface)
{
	const unsigned char inquire_msg[] = {
	  0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78,
	  0x24, 0x00, 0x00, 0x00, 0x80, 0x00, 0x06, 0x12,
	  0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	char *command;
	char data[36];
	int i, ret,message_endpoint,response_endpoint;

	command = malloc(31);
	if (command == NULL) {
		ret = 1;
		goto out;
	}
	ALOGD("Doing SCSI Command Inquiry\n");
	memcpy(command, inquire_msg, sizeof (inquire_msg));

	ret = usb_claim_interface(usb_device_handle, interface);
	if (ret != 0) {
		ALOGD(" Could not claim interface (error %d). Skipping device inquiry\n", ret);
		goto out;
	}
	usb_clear_halt(usb_device_handle, message_endpoint);

	ret = usb_bulk_write(usb_device_handle, message_endpoint, (char *)command, 31, 0);
	if (ret < 0) {
		ALOGD(" Could not send INQUIRY message (error %d)\n", ret);
		goto out;
	}

	ret = usb_bulk_read(usb_device_handle, response_endpoint, data, 36, 0);
	if (ret < 0) {
		ALOGD(" Could not get INQUIRY response (error %d)\n", ret);
		goto out;
	}

	i = usb_bulk_read(usb_device_handle, response_endpoint, command, 13, 0);

	printf("\nSCSI inquiry data (for identification)\n");
	printf("-------------------------\n");

	printf("  Vendor String: ");
	for (i = 8; i < 16; i++) printf("%c",data[i]);
	printf("\n");

	printf("   Model String: ");
	for (i = 16; i < 32; i++) printf("%c",data[i]);
	printf("\n");

	printf("Revision String: ");
	for (i = 32; i < 36; i++) printf("%c",data[i]);

	printf("\n-------------------------\n");

out:
	//if (strlen(message_content) == 0)
		usb_clear_halt(usb_device_handle, message_endpoint);
		usb_release_interface(usb_device_handle, interface);
	free(command);
	return ret;
}
int get_usb_device_configuration(struct usb_dev_handle* device_handle)
{
	int ret =0;
	int configuration = 0;
	char buffer[BUF_SIZE];
	ALOGD("Getting the current device configuration ...\n");
	ret = usb_control_msg(device_handle, USB_DIR_IN + USB_TYPE_STANDARD + USB_RECIP_DEVICE, USB_REQ_GET_CONFIGURATION, 0, 0, buffer, 1, 1000);
	if (ret < 0) {
		// There are quirky devices which fail to respond properly to this command
		ALOGD("Error getting the current configuration (error %d). Assuming configuration 1.\n", ret);
		if (configuration) {
			ALOGD( " No configuration setting possible for this device.\n");
			configuration = 0;
		}
		return 1;
	} else {
		ALOGD(" OK, got current device configuration (%d)\n", buffer[0]);
		return buffer[0];
	}
	return 0;
}
struct usb_device* get_usb_device(int vendor,int product)
{
	struct usb_bus *busses;
	struct usb_bus *bus;
	usb_find_busses();
    	usb_find_devices();    
    	busses = usb_get_busses();
	struct usb_device *return_device = NULL; 
   	for (bus = busses; bus; bus = bus->next) {
    		struct usb_device *dev;
    		for (dev = bus->devices; dev; dev = dev->next) {
    			/* Check if this device is a printer */
			ALOGD("Searching UsbBus (%04x_%04x)  %04x_%04x",dev->descriptor.idVendor,dev->descriptor.idProduct,vendor,product);
			if((dev->descriptor.idVendor==vendor) && (dev->descriptor.idProduct=product))
			{
				ALOGD("Found Usb Device On Bus\n");
				return_device = dev;
				break;
			}	
    		}
    		if( return_device != NULL )
			break;
    	}
    	return return_device;
}