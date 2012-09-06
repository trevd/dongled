LOCAL_PATH:= $(call my-dir)
#----------------------------------------------------------------
# libusb-0.1.12
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        ../libusb-0.1.12/descriptors.c \
        ../libusb-0.1.12/error.c \
        ../libusb-0.1.12/linux.c \
        ../libusb-0.1.12/usb.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../libusb-0.1.12

LOCAL_CFLAGS += -W -Wall
LOCAL_CFLAGS += -fPIC -DPIC


ifeq ($(TARGET_BUILD_TYPE),release)
        LOCAL_CFLAGS += -O2
endif

LOCAL_MODULE:= libusb0
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)


#----------------------------------------------------------------


# dongled executable

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
   uevent.c \
   dongled.c \
   modeswitch.c \

   
LOCAL_C_INCLUDES += \
  $(LOCAL_PATH) \
  $(LOCAL_PATH)/..
      
LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_STATIC_LIBRARIES := \
    libusb0

LOCAL_MODULE := dongled

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
