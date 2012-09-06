#define LOG_TAG "dongled"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <sys/stat.h>
#include <cutils/properties.h>
#include <libusb-android/libusb/libusb.h>
#include "uevent.h"
#include "usb_modeswitch.h"

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
static void try_usb_modeswitch(const char * vendor_id,const char * product_id,const char * modeswitch_d){

        char * config_filename = bprintf("%s/%04s_%04sx",modeswitch_d,vendor_id ,product_id);
        ALOGD("Looking For usb_modeswitch config in location %s",config_filename);
	if(!file_exists(config_filename)){ // usb_modeswitch file not found for this device
	        ALOGD("Config Not Found");
                        return ;
	}
	char * usb_modeswitch_command = bprintf("switch_ms_to_3g:-v0x%04s -p0x%04s -c%s",vendor_id ,product_id,config_filename);
	//start_service(usb_modeswitch_command);							
	free(usb_modeswitch_command);
	free(config_filename);
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


int coldboot(int print_list,const char * modeswitch_d){

        libusb_device                    **devList = NULL;
        libusb_device                    *devPtr = NULL;
        libusb_device_handle             *devHandle = NULL;
        struct libusb_device_descriptor  devDesc;
        unsigned char              strDesc[256];
        ssize_t                    numUsbDevs = 0;      // pre-initialized scalars
        ssize_t                    idx = 0;
        int                        retVal = 0;

        retVal = libusb_init (NULL);
        numUsbDevs = libusb_get_device_list (NULL, &devList);
        
        while (idx < numUsbDevs)
        {
                devPtr = devList[idx];

                if ( (retVal = libusb_open (devPtr, &devHandle) ) != LIBUSB_SUCCESS) 
                        break;


                if ( ( retVal = libusb_get_device_descriptor (devPtr, &devDesc) )!= LIBUSB_SUCCESS)   
                        break;
                        
                char* vendorid = malloc(4);
                char* productid= malloc(4);;
                sprintf(vendorid,"%04x",devDesc.idVendor);
                sprintf(productid,"%04x",devDesc.idProduct);
                if( print_list == 0 )
                        printf("iVendor = %04x idProduct = %04x\n", devDesc.idVendor,devDesc.idProduct);                       
                else
                        try_usb_modeswitch(vendorid, productid,modeswitch_d);                 
                free(vendorid);free(productid);

                ALOGD ("   iVendor = %04x idProduct = %04x\n", devDesc.idVendor,devDesc.idProduct);
                libusb_close (devHandle);
                devHandle = NULL;
                idx++;
                

        }  // end of while loop
        if (devHandle != NULL)
                libusb_close (devHandle);

        libusb_exit (NULL);
        return retVal;
}
int main(int argc, char *argv[])
{
	usb_init();
	 //get_usb_devices();
	//print_configs();
	//ALOGD("Long:%ld",res);
	//return 0;
	ALOGI("Dongled Service Start argc:%d\n",argc);
	start_uevent_polling();
	return 0;
        
}
	        
