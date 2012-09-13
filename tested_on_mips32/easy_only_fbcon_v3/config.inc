#define usbd480fb_INTEPDATASIZE              16

#define usbd480fb_VID                        0x16C0
#define usbd480fb_PID                        0x08A6

#define STREAM_DECODER_DISABLED              0x00
#define STREAM_DECODER_ENABLED               0x06

#define usbd480fb_SET_ADDRESS                0xC0
#define usbd480fb_SET_FRAME_START_ADDRESS    0xC4
#define usbd480fb_SET_BRIGHTNESS             0x81
#define usbd480fb_GET_DEVICE_DETAILS         0x80
#define usbd480fb_SET_STREAM_DECODER         0xC6

#define IOCTL_SET_BRIGHTNESS                 0x10
#define IOCTL_GET_DEVICE_DETAILS             0x20

#define usbd480fb_REFRESH_DELAY              1000 / 100 /* about xx fps, less in practice */
#define usbd480fb_REFRESH_JIFFIES            ((usbd480fb_REFRESH_DELAY * HZ) / 1000)

static struct usb_device_id id_table [] =
{
    {
        .match_flags = USB_DEVICE_ID_MATCH_DEVICE |
                       USB_DEVICE_ID_MATCH_INT_CLASS |
                       USB_DEVICE_ID_MATCH_INT_PROTOCOL,
        .idVendor           = usbd480fb_VID,
        .idProduct          = usbd480fb_PID,
        .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
        .bInterfaceProtocol = 0x00
    },
    { },
};
MODULE_DEVICE_TABLE(usb, id_table);

static int refresh_delay = 10;
module_param(refresh_delay, int, 0);
MODULE_PARM_DESC(refresh_delay, "Delay between display refreshes");

