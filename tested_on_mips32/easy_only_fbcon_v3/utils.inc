static sint16_t usbd480fb_get_device_details(struct usbd480fb *dev)
{
    // TODO: return value handling

    sint16_t result;
    uint8_t  *buffer;

    printk("usbd480fb: geting details ... ");

    buffer = kmalloc(64, GFP_KERNEL);
    if (buffer == NULL)
    {
        //dev_err(&dev->udev->dev, "out of memory\n");
        printk("failed, out of memory\n");
        return 1;
    }

    result = usb_control_msg(dev->udev,
                             usb_rcvctrlpipe(dev->udev, 0),
                             usbd480fb_GET_DEVICE_DETAILS,
                             USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
                             0,
                             0,
                             buffer,
                             64,
                             1000);
    if (result)
    {
        #ifdef __BIG_ENDIAN
        dev->is_fix_endian_used = True;
        #else
        dev->is_fix_endian_used = False;
        #endif
        dev->width          = (uint8_t) buffer[20] | ((uint8_t) buffer[21] << 8);
        dev->height         = (uint8_t) buffer[22] | ((uint8_t) buffer[23] << 8);
        dev->byte_per_pixel = BYTES_PER_PIXEL;                                    /* new */
        dev->area_size      = dev->width * dev->height * dev->byte_per_pixel;     /* new no fix endian */
        dev->area_size      = dev->width * dev->height * dev->byte_per_pixel * 2; /* new fix endian */
        strncpy(dev->device_name, buffer, 20);
        printk("done\n");
    }
    else
    {
        //dev_dbg(&dev->udev->dev, "result = %d\n", result);
        printk("failed\n");
    }

    kfree(buffer);
    return 0;
}

static sint16_t usbd480fb_set_brightness(struct usbd480fb *dev, uint16_t brightness)
{
    // TODO: return value handling, check valid dev?

    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             usbd480fb_SET_BRIGHTNESS,
                             USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
                             brightness,
                             0,
                             NULL,
                             0,
                             1000);
    if (result)
        dev_dbg(&dev->udev->dev, "result = %d\n", result);

    return 0;
}

static sint16_t usbd480fb_set_address(struct usbd480fb *dev, uint16_t addr)
{
    // TODO: return value handling
    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             usbd480fb_SET_ADDRESS,
                             USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
                             addr,
                             addr >> 16,
                             NULL,
                             0,
                             1000);
    if (result)
        dev_dbg(&dev->udev->dev, "result = %d\n", result);

    return 0;
}

static sint16_t usbd480fb_set_frame_start_address(struct usbd480fb *dev, uint16_t addr)
{
    // TODO: return value handling
    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             usbd480fb_SET_FRAME_START_ADDRESS,
                             USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
                             addr,
                             addr >> 16,
                             NULL,
                             0,
                             1000);
    if (result)
        dev_dbg(&dev->udev->dev, "result = %d\n", result);

    return 0;
}

static sint16_t usbd480fb_set_stream_decoder(struct usbd480fb *dev, uint16_t sd)
{
    // TODO: return value handling, check valid dev?

    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             usbd480fb_SET_STREAM_DECODER,
                             USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
                             sd,
                             0,
                             NULL,
                             0,
                             1000);
    if (result)
        dev_dbg(&dev->udev->dev, "result = %d\n", result);

    return 0;
}

static ssize_t show_brightness(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480fb     *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->brightness);
}

static ssize_t set_brightness(struct device *dev, struct device_attribute *attr, const char_t *buf, size_t count)
{
    struct usb_interface *intf      = to_usb_interface(dev);
    struct usbd480fb     *d         = usb_get_intfdata(intf);
    sint16_t             brightness = simple_strtoul(buf, NULL, 10);

    d->brightness = brightness;

    usbd480fb_set_brightness(d, brightness);

    return count;
}

static ssize_t show_width(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480fb     *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->width);
}

static ssize_t show_height(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480fb     *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->height);
}

static ssize_t show_name(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480fb     *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%s\n", d->device_name);
}

static DEVICE_ATTR(brightness, S_IWUGO | S_IRUGO, show_brightness, set_brightness);
static DEVICE_ATTR(width, S_IRUGO, show_width, NULL);
static DEVICE_ATTR(height, S_IRUGO, show_height, NULL);
static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

