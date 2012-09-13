boolean_t is_update_screen = True;
static void usbd480fb_update_what2
(
    struct usbd480fb *dev
)
{
    uint32_t pos;
    uint32_t len;

    pos = 0;
    len = (dev->width * dev->height);
    //usbd480_update_line(dev, pos, len);
}

static void usbd480fb_update_screen
(
    struct usbd480fb *dev
)
{
    uint32_t * vmem_pos;
    uint32_t vmem_len;
    sint16_t result;
    sint16_t sentsize;
    sint16_t writeaddr;
    sint16_t showaddr;

    vmem_len = dev->area_size / 2;
    vmem_pos = (uint32_t *) (dev->vmem + vmem_len);

    if (dev->disp_page == 0)
    {
        writeaddr      = 0;
        showaddr       = 0;
        dev->disp_page = 1;
    }
    else
    {
        writeaddr      = vmem_len;
        showaddr       = vmem_len;
        dev->disp_page = 0;
    }

    if (is_update_screen == False)
    {
        printk(KERN_INFO "usbd480fb: update_screen\n");
        is_update_screen = True;
    }

    usbd480fb_set_stream_decoder(dev, STREAM_DECODER_DISABLED);
    usbd480fb_set_address(dev, writeaddr);

    endian_fix_16bit((uint32_t *) dev->vmem, vmem_pos, vmem_len);
    result = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, 2),
                          vmem_pos,
                          vmem_len,
                          &sentsize,
                          5000
                          );
    usbd480fb_set_frame_start_address(dev, showaddr);
}
