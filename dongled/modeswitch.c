#include <modeswitch.h>
#define BUF_SIZE 4096



int get_current_configuration()
{
        libusb_device_handle             *devHandle = NULL;
        int                        retVal = 0;
        int                        ret = 0;
        unsigned char              buffer[BUF_SIZE];
        ALOGD("init libusb");
        retVal = libusb_init (NULL);
        ALOGD("....yes\n");
        //SHOW_PROGRESS("Getting the current device configuration ...\n");
        ret = libusb_control_transfer(devHandle, LIBUSB_DIR_IN + LIBUSB_REQUEST_TYPE_STANDARD + LIBUSB_RECIPIENT_DEVICE, LIBUSB_REQUEST_GET_CONFIGURATION, 0, 0, buffer, 1, 1000);
        ALOGD("control\n");
         if (devHandle != NULL)
                libusb_close (devHandle);
        libusb_exit (NULL);
        if (ret < 0) {
                fprintf(stderr, "Error: getting the current configuration failed (error %d). Aborting.\n\n", ret);
                exit(1);
        } else {
             fprintf(stdout," OK, got current device configuration (%d)\n", buffer[0]);
            //    SHOW_PROGRESS(" OK, got current device configuration (%d)\n", buffer[0]);
                return buffer[0];
        }
}