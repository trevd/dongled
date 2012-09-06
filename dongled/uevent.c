/*
 * 
 *  uevent.c - handle uevent kernel 
 * 
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
#include "usb_modeswitch.h"
#include "uevent.h"

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
void die(char *s)
{
	ALOGI("dying with %s",s);
	write(2,s,strlen(s));
	exit(1);
}
static void write_uevent_logcat(struct uevent *uevent,const char* label)
{
	ALOGD(" %s { action:'%s' sub:'%s', type:'%s', name:'%s', vendor:%s product:%s\n\t\t\tpath:'%s' }\n",label,uevent->action, uevent->subsystem,uevent->type,uevent->name,uevent->vendor_id ,uevent->product_id,uevent->path);
}
static void handle_uevent(struct uevent *uevent)
{
	if( strlen(uevent->type) == 0 ) 
		return ; // Undefined type  
		
		
	if( !strncmp(uevent->action,"add",strlen(uevent->action)))
		if( !strncmp(uevent->type,"usb_device",strlen(uevent->type)))
			if(!strncmp(uevent->vendor_id,"12d1",strlen(uevent->vendor_id)))
				if(!strncmp(uevent->product_id,"1c0b",strlen(uevent->product_id)))
				{
					// Take a moment
					sleep(1);
					char * argv[] = { "usb_modeswitch","-v0x12d1","-p0x1c0b","-c/etc/usb_modeswitch/12d1_1c0b" } ;
					write_uevent_logcat(uevent,uevent->type);
					usb_modeswitch_main(4,argv);
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