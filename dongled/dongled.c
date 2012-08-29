


#define LOG_TAG "dongled"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/types.h>
#include <linux/netlink.h>
#include <cutils/properties.h>

#include <libusb-android/libusb/libusb.h>
#define LIBUSB_DIR_OUT 0x00
#define LIBUSB_DIR_IN  0x80

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
struct hotplug_info {
	const char *modeswitch_d;
	const char *product_id;
	const char *vendor_id;
	int modeswitch_length;
	int debug;

} ;
void die(char *s)
{
	ALOGI("dying with %s",s);
	write(2,s,strlen(s));
	exit(1);
}
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

static void write_uevent_logcat(struct uevent *uevent,const char* label)
{
	ALOGD(" %s { action:'%s' sub:'%s', type:'%s', name:'%s', vendor:%s product:%s\n\t\t\tpath:'%s' }\n",label,uevent->action, uevent->subsystem,uevent->type,uevent->name,uevent->vendor_id ,uevent->product_id,uevent->path);
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

        char * config_filename = bprintf("%s/%04s_%04s",modeswitch_d,vendor_id ,product_id);
        ALOGD("Looking For usb_modeswitch config in location %s",config_filename);
	if(!file_exists(config_filename)){ // usb_modeswitch file not found for this device
	        ALOGD("Config Not Found");
                        return ;
	}
	char * usb_modeswitch_command = bprintf("switch_ms_to_3g:-v0x%04s -p0x%04s -c%s",vendor_id ,product_id,config_filename);
	start_service(usb_modeswitch_command);							
	free(usb_modeswitch_command);
	free(config_filename);
}
static void handle_event(struct uevent *uevent,struct hotplug_info *hotplug_info)
{
	write_uevent_logcat(uevent,uevent->type);
	//return ;
	int c =uevent->action[0];
	switch(c)
	{
		case 'a':{
			if(!strncmp(uevent->type,"usb_interface",12)){		
				// see if this interface is ripe for a switching
				write_uevent_logcat(uevent,uevent->type);
				try_usb_modeswitch(uevent->vendor_id, uevent->product_id,hotplug_info->modeswitch_d);
				
			}else if(!strncmp(uevent->type,"usb-serial",10)){
				//Serial Killals
				write_uevent_logcat(uevent,uevent->type);
			} else if(!strncmp(uevent->subsystem,"tty",3)){
				write_uevent_logcat(uevent,uevent->type);
				if(!strncmp(uevent->name,"ttyUSB0",7))
       					property_set("ril.pppd_tty", "/dev/ttyUSB0");
                                if(!strncmp(uevent->name,"ttyHS4",7) )
					property_set("ril.pppd_tty", "/dev/ttyHS4");
				
				if(!strncmp(uevent->name,"ttyHS3",7)){
					property_set("rild.libargs", "-d /dev/ttyHS3");
					property_set("rild.libpath", "/system/lib/libtcl-ril.so");
					start_service("ril-daemon");
				}
				if(!strncmp(uevent->name,"ttyUSB2",7))
				{
					property_set("rild.libargs", "-d /dev/ttyUSB2");
					property_set("rild.libpath", "/system/lib/libhuaweigeneric-ril.so");
					start_service("ril-daemon");
				}	
				
			}else {
				//write_uevent_logcat(uevent,"ignored");
			}
			
			break;
		}
		case 'r':{
			if(!strncmp(uevent->type,"usb_device",10)){
				//Serial Killa
				write_uevent_logcat(uevent,uevent->type);
				//handle_connection_connection();	
			} else if(!strncmp(uevent->subsystem,"tty",3)){
				write_uevent_logcat(uevent,uevent->type);
				if(!strncmp(uevent->name,"ttyUSB2",7))
				{
					ALOGD("Stopping Rild");	
					stop_service("ril-daemon");
				}else if(!strncmp(uevent->name,"ttyUSB0",7)){
					system("pkill -9 ppp");
					ALOGD("Killing PPP");	
				}
			}
			break;
		}
		case 'c':{
			//write_uevent_logcat(uevent,"ignored");
		}
			break;
	}
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
static void parse_event(const char *msg, struct uevent *uevent,int debug)
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
int switch_main(int argc, char *argv[])
{
        ALOGD("Dongled Service Start argc:%d\n",argc);
        get_current_configuration();
        return 0;
        struct hotplug_info hotplug_info;
	parse_hotplug_info(&hotplug_info);
	if(argc == 2){
	        int print= strncmp(argv[1],"list-usb",strlen("list-usb"));
        	if(  print==0 || (!strncmp(argv[1],"checknow",strlen("checknow") )))
	        {
	                ALOGD("Warm me up then Checking Bus with argc:%d argv[1]=%s",argc,argv[1]);
               	        printf("Running dongled as preheated - looking for existing devicesmodeswitch.d:%s length:%u debug:%u\n",hotplug_info.modeswitch_d,hotplug_info.modeswitch_length,hotplug_info.debug);
        		return coldboot(print,hotplug_info.modeswitch_d);
        	}       
        	return 0;
        }
	struct sockaddr_nl nls;
	struct pollfd pfd;
	char uevent_msg[1024];
	// Open hotplug event netlink socket
	ALOGI("Starting dongled For UsbModeSwitch Management - Settings:modeswitch.d:%s length:%u debug:%u",hotplug_info.modeswitch_d,hotplug_info.modeswitch_length,hotplug_info.debug);
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
	        parse_event(uevent_msg, &uevent,hotplug_info.debug);
	        handle_event(&uevent,&hotplug_info);
	}
	die("poll error\n");

	// Dear gcc: shut up.
	return 0;
}
	        
