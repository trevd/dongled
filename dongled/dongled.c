/*
 *  dongled.c - contains the entry point for the dongled native service
 *  this handles pre-initialization of usb dongles. The dongled service can 
 *  be configuration to use external configuration files as well as an external usb_modeswitch
 *  binary or service.
 *  ro.dongled.external.usb_modeswitch = 1
 *  ro.dongled.external.override = 1
 *   
 */
#define LOG_TAG "dongled"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <sys/stat.h>
#include <pthread.h>
#include <cutils/properties.h>
#include <usbhost/usbhost.h>
//#include "uevent.h"
void check_usb_devices();
pthread_t s_tid_coldboot;
void die(char *s);

struct hotplug_info {
	const char *modeswitch_d;
	const char *product_id;
	const char *vendor_id;
	int modeswitch_length;
	int debug;

} ;

char *bprintf(const char *fmt, ...)
{
	char *strp = NULL;

	va_list args;
	va_start(args, fmt);
	vasprintf(&strp, fmt, args);
	va_end(args);

	return strp;
}

static int file_exists(char *filename)
{
	struct stat sb;
	if(stat(filename, &sb) == -1) {
		ALOGD("%s Not Found",filename);
		return 0 ; 
	}
	ALOGD("%s Found",filename);
	return 1;
}
static int test_directory(struct hotplug_info *hotplug_info,const char * path)
{
	int isDir = 0;
	struct stat statbuf;
	if (stat(path, &statbuf) != -1) {
	   if (S_ISDIR(statbuf.st_mode)) {
	   	hotplug_info->modeswitch_d = path;
   		ALOGD("%s found",path);
		isDir = 1;
	   } 
	else
		ALOGD("%s not found",path);
	}
	return isDir;
}

static void find_modeswitch_directory(struct hotplug_info *hotplug_info)
{
	ALOGD("hotplug.modeswitch.d not set...checking known locations");
	if(test_directory(hotplug_info,"/etc/usb_modeswitch")) return;
	if(test_directory(hotplug_info,"/system/etc/usb_modeswitch")) return;
	if(test_directory(hotplug_info,"/system/etc/usb_modeswitch.d")) return;
	die("hotplug.modeswitch.d");

	
}
static int wait_for_property(const char *name, const char *desired_value, int maxwait)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int maxnaps = maxwait / 1;
    if (maxnaps < 1) 
        maxnaps = 1;
   
    while (maxnaps-- > 0) {
        usleep(1000000);
        if (property_get(name, value, NULL)) {
            if (desired_value == NULL ||
                    strcmp(value, desired_value) == 0) {
                return 0;
            }
        }
    }
    return -1; /* failure */
}


static int property_test(const char* name,char* testvalue )
{
	char value[PROPERTY_VALUE_MAX];
	// check to see if this is a device we are interested
	ALOGD("Checking value of %s",name);
	if(!property_get(name,value,NULL))
	{
		ALOGD("Property Not Found");
		return 2;
	}
	ALOGD("Comparing String %s %s",value,testvalue);
	int res = strncmp(value,testvalue,strlen(testvalue));
	ALOGD("Result of Comparison %u",res);
	return res;
}



 
static void parse_hotplug_info(struct hotplug_info *hotplug_info)
{
	char value[PROPERTY_VALUE_MAX];
	if(property_get("hotplug.modeswitch.d",value,""))
		hotplug_info->modeswitch_d = value;
	else
	  find_modeswitch_directory(hotplug_info);		
	  
	hotplug_info->modeswitch_length = strlen(hotplug_info->modeswitch_d);
	
	char debug[PROPERTY_VALUE_MAX];
	if(!property_get("hotplug.debug",debug,"0"))
	hotplug_info->debug =atoi(debug);
}

static void *coldboot(void *param){
		
	// This needs to be fired off on another thread
	ALOGI("Dongled Service ColdBoot\n");
	//check_usb_devices();
	return 0;

}
static int dongled_device_removed(void *client_data)
{
	ALOGI("Dongled Device Removed\n");
	return 0;
}
static int lsusb_discovery_done(void *client_data)
{
    ALOGI("Dongled Device Discovery Done\n");
    return 0;
}
static int dongled_device_added(const char *dev_name, void *client_data)
{
	struct usb_device *dev = usb_device_open(dev_name);

    ALOGI("Dongled Device Added Opening USB Device %s\n",dev_name);
    if (!dev) {
        ALOGE("can't open device %s: %s\n", dev_name, strerror(errno));
        return 0;
    }

   
        uint16_t vid, pid;
        char *mfg_name, *product_name, *serial;

        vid = usb_device_get_vendor_id(dev);
        pid = usb_device_get_product_id(dev);
        mfg_name = usb_device_get_manufacturer_name(dev);
        product_name = usb_device_get_product_name(dev);
        serial = usb_device_get_serial(dev);

        ALOGI("%s: %04x:%04x %s %s %s\n", dev_name, vid, pid,mfg_name, product_name, serial);

        free(mfg_name);
        free(product_name); 
        free(serial);
    usb_device_close(dev);

    return 0;
}
int main(int argc, char *argv[])
{
	//usb_init();
	ALOGI("Dongled Service Start argc:%d\n",argc);
	struct usb_host_context *ctx;
	ctx = usb_host_init();
	if (!ctx) {
		perror("usb_host_init:");
		return 1;
	}else {
		ALOGI("Got usb_host_context\n");
	}
	usb_host_run(ctx,dongled_device_added,dongled_device_removed, lsusb_discovery_done,NULL);
	//pthread_create(&s_tid_coldboot, NULL, coldboot, NULL);
	 usb_host_cleanup(ctx);
	ALOGI("cleaning up usb_host_context\n");
	//start_uevent_polling();
	return 0;
        
}
	        
