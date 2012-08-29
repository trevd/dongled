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

#include <libusb.h>
#define LIBUSB_DIR_OUT 0x00
#define LIBUSB_DIR_IN  0x80

int get_current_configuration();