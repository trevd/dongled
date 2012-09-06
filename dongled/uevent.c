/*
 * 
 *  uevent.c - handle uevent kernel notifications
 *  The processing of the uevent notifications is kept stateless
 *  we are not dependent on previous uevent. 
 */

#define LOG_TAG "dongled"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <cutils/log.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <unistd.h>

#include <linux/types.h>
#include <linux/netlink.h>
#include <cutils/properties.h>
#include <libusb-0.1.12/usb.h>
#include "usb_modeswitch.h"
#include "uevent.h"
#define DESCR_MAX 129
struct uevent { 
	const char *action;
	const char *path;
	const char *subsystem;
	const char *firmware;
	const char *partition_name;
	const char *name;
	const char *type;
	const char *product_id;
	const char *vendor_id;
	int partition_num;
	int major;
	int minor;
	unsigned int seqnum;
};
struct usb_modeswitch_config {
	int vendor;
	int product;
	char *message;
} usbmsconfigs[] = { 0x12d1,0x1c0b,"55534243123456780000000000000011062000000100000000000000000000" };
struct usb_dev_handle *devh;
void die(char *s)
{
	ALOGI("dying with %s",s);
	write(2,s,strlen(s));
	exit(1);
}
int find_config(int vendor,int product)
{	
	int counter = 0,totalconfigs=1, returnvalue =-1;
	for(counter =0; counter < totalconfigs ;counter ++)
	{
		if((usbmsconfigs[counter].vendor==vendor) && (usbmsconfigs[counter].product==product))
		{
			returnvalue = counter;
			break;
		}
	}
	return returnvalue;
}
int get_int_from_hexstring(const char* hexString)
{
	// chuck it on the heap, why not eh?
	char* hexString2 = malloc(6);
	// pad out the hexString and make sure strtol knows it's hex
	sprintf(hexString2,"0x%4s",hexString);
	int result = (int)strtol(hexString2,NULL,0);
	free(hexString2);
	return result;
}
void print_configs(){
	ALOGD("C:%d,%d,%s\n",usbmsconfigs[0].vendor,usbmsconfigs[0].product,usbmsconfigs[0].message);
	fprintf(stdout,"C:%x,%x,%s\n",usbmsconfigs[0].vendor,usbmsconfigs[0].product,usbmsconfigs[0].message);
}
void get_usb_devices()
{
	
	char iproduct[DESCR_MAX];
	struct usb_bus *busses;
	struct usb_bus *bus;
	usb_find_busses();
    	usb_find_devices();
    
    	busses = usb_get_busses();
    	int c, i, a, ret;
    	for (bus = busses; bus; bus = bus->next) {
    		struct usb_device *dev;
    
    		for (dev = bus->devices; dev; dev = dev->next) {
    			/* Check if this device is a printer */
			ALOGD("Dev DeviceClass %04x %04x\n",dev->descriptor.idVendor,dev->descriptor.idProduct);
			fprintf(stdout,"Dev DeviceClass %04x %04x\n",dev->descriptor.idVendor,dev->descriptor.idProduct);
			
    			if (dev->descriptor.bDeviceClass == 7) {
    				/* Open the device, claim the interface and do your processing */
    			}
    
    			/* Loop through all of the configurations */
    			for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
    				/* Loop through all of the interfaces */
    				for (i = 0; i < dev->config[c].bNumInterfaces; i++) {
    					/* Loop through all of the alternate settings */
    					for (a = 0; a < dev->config[c].interface[i].num_altsetting; a++) {
    						/* Check if this interface is a printer */
    						if (dev->config[c].interface[i].altsetting[a].bInterfaceClass == 7) {
    							/* Open the device, set the alternate setting, claim the interface and do your processing */
    						}
    					}
    				}
    			}
    		}
    	}
}
static void start_service(const char * service_name)
{
	property_set("ctl.start",service_name);
	ALOGD("starting %s",service_name);
}
static void stop_service(const char * service_name)
{
	property_set("ctl.stop",service_name);
	ALOGD("stop %s",service_name);
}
static void write_uevent_logcat(struct uevent *uevent,const char* label)
{
	ALOGD(" %s { action:'%s' sub:'%s', type:'%s', name:'%s', vendor:%s product:%s\n\t\t\tpath:'%s' }\n",label,uevent->action, uevent->subsystem,uevent->type,uevent->name,uevent->vendor_id ,uevent->product_id,uevent->path);
}
static void handle_uevent(struct uevent *uevent)
{
	
	int c =uevent->action[0];
	switch(c)
	{
		case 'a':{
			if( strlen(uevent->type) == 0 )	{
				// if no type is specified then look for a tty subsystem 
				if (  strlen(uevent->subsystem) == 0 ){ // handle zero length subsystem
					return ; // Undefined type
				}else if( !strncmp(uevent->subsystem,"tty",strlen(uevent->subsystem))){
					if(!strncmp(uevent->name,"ttyUSB0",7)){
						property_set("ril.pppd_tty", "/dev/ttyUSB0");
						return ;
					}
					if(!strncmp(uevent->name,"ttyUSB2",7))	{
						property_set("rild.libargs", "-d /dev/ttyUSB2");
						property_set("rild.libpath", "/system/lib/libhuaweigeneric-ril.so");
						stop_service("ril-daemon");
						sleep(1);
						start_service("ril-daemon");
						return;
					}	
				}
				return;
			}else{
				if( !strncmp(uevent->type,"usb_device",strlen(uevent->type)))
				{
					
					int vendor = get_int_from_hexstring(uevent->vendor_id);
					int product = get_int_from_hexstring(uevent->product_id);
					int config_index = find_config(vendor,product);
					ALOGD("Config Index:%d",config_index);
					return;
					
					//if(!strncmp(uevent->vendor_id,"12d1",strlen(uevent->vendor_id)))
					//	if(!strncmp(uevent->product_id,"1c0b",strlen(uevent->product_id)))
					//	{
					//		sleep(1); // Take a moment
					//		char * argv[] = { "usb_modeswitch","-v0x12d1","-p0x1c0b","-c/etc/usb_modeswitch/12d1_1c0b" } ;
					//		write_uevent_logcat(uevent,uevent->type);
					//		 get_usb_devices();
					//		usb_modeswitch_main(4,argv);
					//	}
				}
			}
		}
	}
					
				
	
	return ;
}
static void parse_uevent(const char *msg, struct uevent *uevent)
{
	uevent->action = "";
	uevent->path = "";
	uevent->name = "";
	uevent->type = "";
	uevent->subsystem = "";
	uevent->firmware = "";
	uevent->major = -1;
	uevent->minor = -1;
	uevent->partition_name = NULL;
	uevent->partition_num = -1;
	uevent->vendor_id = "";
	uevent->product_id = "";

	while(*msg) {
		if(!strncmp(msg, "ACTION=", 7)) {
		    msg += 7;
		    uevent->action = msg;
		} else if(!strncmp(msg, "DEVPATH=", 8)) {
		    msg += 8;
		    uevent->path = msg;
		} else if(!strncmp(msg, "DEVNAME=", 8)) {
		    msg += 8;
		    uevent->name = msg;
		} else if(!strncmp(msg, "DEVTYPE=", 8)) {
		    msg += 8;
		    uevent->type = msg;
		} else if(!strncmp(msg, "SUBSYSTEM=", 10)) {
		    msg += 10;
		    uevent->subsystem = msg;
		} else if(!strncmp(msg, "FIRMWARE=", 9)) {
		    msg += 9;
		    uevent->firmware = msg;
    		} else if(!strncmp(msg, "MAJOR=", 6)) {
		    msg += 6;
		    uevent->major = atoi(msg);
		} else if(!strncmp(msg, "MINOR=", 6)) {
		    msg += 6;
		    uevent->minor = atoi(msg);
		} else if(!strncmp(msg, "PARTN=", 6)) {
		    msg += 6;
		    uevent->partition_num = atoi(msg);
		} else if(!strncmp(msg, "PARTNAME=", 9)) {
		    msg += 9;
		    uevent->partition_name = msg;
		} else if (!strncmp(msg, "PRODUCT=", 8)) {
		   	msg += 8 ;
		   	char * pch;
		   	pch = strtok ((char *)msg,"/");
			if(pch != NULL)
	  			uevent->vendor_id = pch;
	  		pch = strtok (NULL,"/");
	  		if(pch != NULL)
	  			uevent->product_id = pch;
		}		
		    /* advance to after the next \0 */
		while(*msg++);
	}
	return ;
}
void start_uevent_polling()
{
	struct sockaddr_nl nls;
	struct pollfd pfd;
	char uevent_msg[1024];
	// Open hotplug event netlink socket
	//ALOGI("Starting dongled For UsbModeSwitch Management - Settings:modeswitch.d:%s length:%u debug:%u",hotplug_info.modeswitch_d,hotplug_info.modeswitch_length,hotplug_info.debug);
	memset(&nls,0,sizeof(struct sockaddr_nl));
	nls.nl_family = AF_NETLINK;
	nls.nl_pid = getpid();
	nls.nl_groups = -1;  


	pfd.events = POLLIN;
	pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (pfd.fd==-1)
		ALOGE("dongled cannot create socket, are you root?\n");

	// Listen to netlink socket

	if (bind(pfd.fd, (void *)&nls, sizeof(struct sockaddr_nl)))
		die("bind failed\n");
	ALOGD("Give us a go on your UEVENT");
	while (-1!=poll(&pfd, 1, -1)) {

		int uevent_buffer_length = recv(pfd.fd, uevent_msg, sizeof(uevent_msg), MSG_DONTWAIT);
		if (uevent_buffer_length == -1) 
			die("receive error\n");
		struct uevent uevent;
	        parse_uevent(uevent_msg, &uevent);
		handle_uevent(&uevent);
	}
	die("poll error\n");
	return;
}