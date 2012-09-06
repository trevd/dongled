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
#include <libusb-0.1.12/usb.h>
#include "modeswitch.h"
#define DESCR_MAX 129
#define USB_DIR_OUT 0x00
#define USB_DIR_IN  0x80
#define BUF_SIZE 4096
#define LINE_DIM 1024

int hex2num(char c)
{
	if (c >= '0' && c <= '9')
	return c - '0';
	if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
	return -1;
}


int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int hexstr2bin(const char *hex, char *buffer, int len)
{
	int i;
	int a;
	const char *ipos = hex;
	char *opos = buffer;

	for (i = 0; i < len; i++) {
	a = hex2byte(ipos);
	if (a < 0)
		return -1;
	*opos++ = a;
	ipos += 2;
	}
	return 0;
}

int write_bulk(usb_dev_handle* usb_device_handle,int endpoint, char *message, int length)
{
	int ret;
	ALOGD("In Write Bulk\n");
	ret = usb_bulk_write(usb_device_handle, endpoint, message, length, 3000);
	if (ret >= 0 ) {
		ALOGD(" OK, message successfully sent\n");
	} else
		if (ret == -19) {
			ALOGD(" Device seems to have vanished right after sending. Good.\n");
		} else
			ALOGD(" Sending the message returned error %d. Trying to continue\n", ret);
	return ret;

}

int read_bulk(usb_dev_handle* usb_device_handle,int endpoint, char *buffer, int length)
{
	int ret;
	ret = usb_bulk_read(usb_device_handle, endpoint, buffer, length, 3000);
	usb_bulk_read(usb_device_handle, endpoint, buffer, 13, 100);
	if (ret >= 0 ) {
		ALOGD("  OK, response successfully read (%d bytes).\n", ret);
	} else
		if (ret == -19) {
			ALOGD(" Device seems to have vanished after reading. Good.\n");
		} else
			ALOGD(" Response reading got error %d\n", ret);
	return ret;

}
int usb_send_message(usb_dev_handle* usb_device_handle,char* message,int message_endpoint)
{
	int message_length, ret;
	char byte_string[LINE_DIM/2];
	ALOGD("In Send Message\n");
	if (strlen(message) % 2 != 0) {
		ALOGD( "Error: MessageContent hex string has uneven length. Skipping ...\n");
		return 1;
	}
	message_length = strlen(message) / 2;
	if ( hexstr2bin(message, byte_string, message_length) == -1) {
		ALOGD( "Error: MessageContent %s\n is not a hex string. Skipping ...\n",  message);
		return 1;
	}
	ALOGD("Trying to send message to endpoint 0x%02x ...\n",  message_endpoint);
	ret = write_bulk(usb_device_handle,message_endpoint, byte_string, message_length);
	if (ret == -19)
		return 1;

	return 0;
}

int usb_switch_send_message (usb_dev_handle* usb_device_handle,int inquire_device,int switch_configuration_index,int interface,int message_endpoint,int response_endpoint)
{
	const char* command_header = "55534243";
	int release_delay =0 ;
	int ret, i;
	char* message_content = switch_configurations[switch_configuration_index].message;
	

	/* May be activated in future versions */
//	if (MessageContent2[0] != '\0' || MessageContent3[0] != '\0')
//		NeedResponse = 1;
	ALOGD("switchSendMessage\n");
	ALOGD("Setting up communication with interface %d\n", interface);
	if (inquire_device != 2) {
		ALOGD("Inq !=2\n");
		ret = usb_claim_interface(usb_device_handle, interface);
		if (ret != 0) {
			ALOGD(" Could not claim interface (error %d). Skipping message sending\n", ret);
			return 0;
		}
	}
	ALOGD("Msg=%s\n",message_content);
	usb_clear_halt(usb_device_handle, message_endpoint);
	ALOGD("Using endpoint 0x%02x for message sending ...\n", message_endpoint);
	if ( usb_send_message(usb_device_handle,message_content,message_endpoint) )
	{
		goto skip;

		//if (NeedResponse) {
		//	if ( strstr(message_content,command_header) != NULL ) {
				// UFI command
		//		ALOGD("Reading the response to message %d (CSW) ...\n", i+1);
		//		ret = read_bulk(response_endpoint, ByteString, 13);
		//	} else {
		//		// Other bulk transfer
		//		ALOGD("Reading the response to message %d ...\n", i+1);
		//		ret = read_bulk(response_endpoint, ByteString, strlen(message_content)/2 );
		//	}
		//	if (ret < 0)
		//		goto skip;
		//}
	}

	ALOGD("Resetting response endpoint 0x%02x\n", response_endpoint);
	ret = usb_clear_halt(usb_device_handle, response_endpoint);
	if (ret)
		ALOGD(" Could not reset endpoint (probably harmless): %d\n", ret);
	ALOGD("Resetting message endpoint 0x%02x\n", message_endpoint);
	ret = usb_clear_halt(usb_device_handle,  message_endpoint);
	if (ret)
		ALOGD(" Could not reset endpoint (probably harmless): %d\n", ret);
	usleep(200000);
	if (release_delay) {
		ALOGD("Blocking the interface for %d ms before releasing ...\n", release_delay);
		usleep(release_delay*1000);
	}
	ret = usb_release_interface(usb_device_handle, interface);
	if (ret)
		goto skip;
	return 1;

skip:
	ALOGD(" Device is gone, skipping any further commands\n");
	//usb_close(usb_device_handle);
	//usb_device_handle = 0;
	return 2;
}
int detach_device_driver(usb_dev_handle* usb_device_handle,int interface)
{
	int ret;
	char buffer[BUF_SIZE];
	ALOGD("Looking for active driver ...\n");
	ret = usb_get_driver_np(usb_device_handle, interface, buffer, BUF_SIZE);
	if (ret != 0) {
		ALOGD(" No driver found. Either detached before or never attached\n");
		return 1;
	}
	if (strncmp("dummy",buffer,5) == 0) {
		ALOGD(" OK, driver found; name unknown, limitation of libusb1\n");
		strcpy(buffer,"unkown");
	} else {
		ALOGD(" OK, driver found (\"%s\")\n", buffer);
	}
	ret = usb_detach_kernel_driver_np(usb_device_handle, interface);
	if (ret == 0) {
		ALOGD(" OK, driver \"%s\" detached\n", buffer);
	} else
		ALOGD(" Driver \"%s\" detach failed with error %d. Trying to continue\n", buffer, ret);
	return 1;
}
int find_first_bulk_output_endpoint(struct usb_device *usb_device)
{
	int i;
	struct usb_interface_descriptor *alt = &(usb_device->config[0].interface[0].altsetting[0]);
	struct usb_endpoint_descriptor *ep;

	for(i=0;i < alt->bNumEndpoints;i++) {
		ep=&(alt->endpoint[i]);
		if( ( (ep->bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK) &&
		    ( (ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT ) ) {
			return ep->bEndpointAddress;
		}
	}
	return 0;
}


int find_first_bulk_input_endpoint(struct usb_device *usb_device)
{
	int i;
	struct usb_interface_descriptor *alt = &(usb_device->config[0].interface[0].altsetting[0]);
	struct usb_endpoint_descriptor *ep;

	for(i=0;i < alt->bNumEndpoints;i++) {
		ep=&(alt->endpoint[i]);
		if( ( (ep->bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK) &&
		    ( (ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN ) ) {
			return ep->bEndpointAddress;
		}
	}

	return 0;
}
/* Print result of SCSI command INQUIRY (identification details) */
int usb_device_get_interface_class(struct usb_device *usb_device, int configuration_number, int interface_number)
{
	int i;
	int j;
	// some single-configuration devices balk on iteration, treat them separately
	if (configuration_number == 0)
		for (i=0; i<usb_device->config[0].bNumInterfaces; i++) {
			if (usb_device->config[0].interface[i].altsetting[0].bInterfaceNumber == interface_number)
				return usb_device->config[0].interface[i].altsetting[0].bInterfaceClass;
		}
	else
		for (j=0; j<usb_device->descriptor.bNumConfigurations; j++)
			if (usb_device->config[j].bConfigurationValue == configuration_number)
				for (i=0; i<usb_device->config[j].bNumInterfaces; i++) {
					if (usb_device->config[j].interface[i].altsetting[0].bInterfaceNumber == interface_number)
						return usb_device->config[j].interface[i].altsetting[0].bInterfaceClass;
				}
	return -1;
}
/* Print result of SCSI command INQUIRY (identification details) */
int usb_device_scsi_inquiry (struct usb_dev_handle *usb_device_handle,int switch_configuration_index ,int interface,int message_endpoint,int response_endpoint)
{
	const unsigned char inquire_msg[] = {
	  0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78,
	  0x24, 0x00, 0x00, 0x00, 0x80, 0x00, 0x06, 0x12,
	  0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	char *command;
	char data[36];
	int i, ret;

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

	ALOGD("\nSCSI inquiry data (for identification)\n");
	ALOGD("-------------------------\n");

	ALOGD("  Vendor String: ");
	for (i = 8; i < 16; i++) printf("%c",data[i]);
	ALOGD("\n");

	ALOGD("   Model String: ");
	for (i = 16; i < 32; i++) printf("%c",data[i]);
	ALOGD("\n");

	ALOGD("Revision String: ");
	for (i = 32; i < 36; i++) printf("%c",data[i]);

	ALOGD("\n-------------------------\n");

out:
	if (strlen( switch_configurations[switch_configuration_index].message) == 0)
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
int get_usb_switch_configuration_index(int vendor,int product)
{	
	int counter = 0,totalconfigs=2, returnvalue =-1;
	for(counter =0; counter < totalconfigs ;counter ++)
	{
		if((switch_configurations[counter].vendor==vendor) && (switch_configurations[counter].product==product))
		{
			returnvalue = counter;
			break;
		}
	}
	return returnvalue;
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
// This is where the modeswitching magic happens - we want to encapulate the usb_modeswitching logic
// into this function which is called from handle_uevent in uevent.c  
void process_add_usb_device_uevent(int vendor,int product)
{
	// Check if we have an internal configuration for this device
	int switch_configuration_index = get_usb_switch_configuration_index(vendor,product);
	if(switch_configuration_index == -1){
		ALOGD("no internal usb_switch_configuration_index found for usb_device %04x_%04x",vendor,product);	
		return ;
	}
	ALOGD("get_usb_switch_configuration_index:%d",switch_configuration_index);
	// we've got a config for this usb devices, try to get an handle on it
	struct usb_device* usb_device = get_usb_device(vendor,product);
	if(usb_device == NULL)
	{
		ALOGD("usb_device not found on bus %04x_%04x maybe we need to sleep for a bit longer",vendor,product);
		return ;
	}
	ALOGD("usb_device %04x_%04x found on bus",vendor,product);
	struct usb_dev_handle* usb_device_handle = usb_open(usb_device);
	if(usb_device_handle == NULL)
	{
		ALOGD("usb_open failed for usb_device %04x_%04x",vendor,product);
		return ;
	}
	ALOGD("usb_open sucess for usb_device %04x_%04x",vendor,product);
	int interface = usb_device->config[0].interface[0].altsetting[0].bInterfaceNumber;
	int default_class = usb_device->descriptor.bDeviceClass;
	ALOGD("usb_device %04x_%04x using interface %d",vendor,product,interface);
	int usb_device_configuration = get_usb_device_configuration(usb_device_handle);
	ALOGD("usb_device %04x_%04x using configuration %d",vendor,product,usb_device_configuration);
	int interface_class = usb_device_get_interface_class(usb_device,usb_device_configuration,interface);
	ALOGD("usb_device %04x_%04x interface_class %d",vendor,product,interface_class );
	int message_endpoint = find_first_bulk_output_endpoint(usb_device);
	ALOGD("usb_device %04x_%04x  message_endpoint %x",vendor,product, message_endpoint );
	int response_endpoint = find_first_bulk_input_endpoint(usb_device);
	ALOGD("usb_device %04x_%04x  response_endpoint %x",vendor,product, response_endpoint );
	detach_device_driver(usb_device_handle,interface);
	int inquire_device = usb_device_scsi_inquiry(usb_device_handle,switch_configuration_index,interface,message_endpoint,response_endpoint);
	usb_switch_send_message(usb_device_handle,inquire_device,switch_configuration_index,interface,message_endpoint,response_endpoint);
	if(!usb_close(usb_device_handle))
	{
		ALOGD("usb_close successful For %04x_%04x",vendor,product);
		return ;
	}
	return;
}