// internal usb_modeswitch configurations - this can be overwritten by setting
// ro.dongled.external_config = 1
struct usb_modeswitch_config {
	int vendor;
	int product;
	char *message;
} switch_configurations[] = { 0x12d1,0x1c0b,"55534243123456780000000000000011062000000100000000000000000000",
	0x1bbb,0xf000,"55534243123456788000000080000606f50402527000000000000000000000" };

