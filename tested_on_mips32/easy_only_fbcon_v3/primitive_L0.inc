boolean_t is_update_rect      = False;
uint32_t  update_rect_counter = 0;
void usbd480fb_update_rect
(
    struct usbd480fb *dev,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height
)
{
    /*
       This implementation is limited to using a 64K preallocated usb buffer.
       Also this implementation just discards the extra data if it doesn't fit
       in single 64K buffer.
       TODO: if 64K isn't enough, use the buffer several times
       TODO: Switch to using urbs instead of usb_bulk_msg
     */

    sint16_t usb_buffer_size;
    sint16_t result;
    sint16_t sentsize;
    sint16_t wraplength;
    sint16_t writeaddress;
    sint16_t writelength;
    sint16_t writesize;
    sint16_t ypos;
    uint8_t  *bufsrc;
    uint8_t  *bufdst;
    sint16_t bufsize;
    sint16_t copylength;
    uint16_t *myscr;
    uint8_t  *myvideomemory;
    uint32_t myx, myy;
    uint32_t i, myindex;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* skipping, too heavy */
    //usbd480fb_update_screen(dev); /* page0 */
    //usbd480fb_update_screen(dev); /* page1 */
    //return;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    /*
     * command
     */

    usb_buffer_size = 1024 * 64;

    if (is_update_rect == False)
    {
        //printk("usbd480fb: update_rect using usb_buffer of %d bytes\n", usb_buffer_size);
        printk("usbd480fb: update_rect event.%ld pos=(%d,%d) size(%d,%d) area %d\n",
               update_rect_counter, x, y, width, height, width * height);
        //is_update_rect = True;
    }

    usbd480fb_set_stream_decoder(dev, STREAM_DECODER_ENABLED);

    wraplength = width - 1;

    dev->cmd_buf[0] = 0x43;     // wrap length
    dev->cmd_buf[1] = 0x5B;
    dev->cmd_buf[2] = wraplength;
    dev->cmd_buf[3] = wraplength >> 8;

    writeaddress = ((y * dev->width) + x);
    writelength  = width * height - 1;

    if (writelength > (usb_buffer_size / 2) - 1)
    {
        writelength = (usb_buffer_size / 2) - 1;
    }
    dev->cmd_buf[4]  = 0x41;    // pixel write
    dev->cmd_buf[5]  = 0x5B;
    dev->cmd_buf[6]  = writeaddress;
    dev->cmd_buf[7]  = writeaddress >> 8;
    dev->cmd_buf[8]  = writeaddress >> 16;
    dev->cmd_buf[9]  = writeaddress >> 24;
    dev->cmd_buf[10] = writelength;
    dev->cmd_buf[11] = writelength >> 8;
    dev->cmd_buf[12] = writelength >> 16;
    dev->cmd_buf[13] = writelength >> 24;

    writesize = 14;

    result = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, 2),
                          dev->cmd_buf,
                          writesize,
                          &sentsize,
                          5000
                          );
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    /*
     * data
     */
    writesize = width * height * dev->byte_per_pixel;

    if (writesize > usb_buffer_size)
    {
        writesize = usb_buffer_size;
    }

    bufsize    = usb_buffer_size;
    copylength = width * dev->byte_per_pixel;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /*
     * myhere
     */
    myscr         = dev->scr;
    myvideomemory = dev->fbinfo->screen_base;

    i = 0;
    for (myy = y; myy < y + height; myy++)
    {
        for (myx = x; myx < x + width; myx++)
        {
            myindex     = (myy * dev->width + myx) * dev->byte_per_pixel;
            dev->scr[i] = myvideomemory[ myindex ];
            dev->scr[i] = swap16(dev->scr[i]) | 0x100;
            i++;
        }
    }
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    bufsrc  = dev->vmem;
    bufsrc  = dev->fbinfo->screen_base;
    bufsrc  = dev->scr;
    bufsrc += (y * dev->width + x) * dev->byte_per_pixel;

    bufdst = dev->usb_buf;

    for (ypos = y; ypos < y + height; ypos++)
    {
        if (bufsize > copylength)    // make sure the line fits in the usb buffer
        {
            memcpy(bufdst, bufsrc, copylength);
        }
        else
        {
            break;
        }

        bufsize -= copylength;
        bufdst  += copylength;
        bufsrc  += dev->fbinfo->fix.line_length;
    }

    result = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, 2),
                          dev->usb_buf,
                          writesize,
                          &sentsize,
                          5000
                          );
}

void myfb_gram_update
(
    struct usbd480fb *dev,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height
)
{
    usbd480fb_update_rect(dev, x, y, width, height);
}


boolean_t is_update_line = False;
void usbd480fb_update_line
(
    struct usbd480fb *dev,
    uint16_t pos,
    uint16_t len
)
{
    /*
       This implementation is limited to using a 64K preallocated usb buffer.
       TODO: Switch to using urbs instead of usb_bulk_msg
     */
    sint16_t usb_buffer_size;
    sint16_t result;
    sint16_t sentsize;
    sint16_t wraplength;
    sint16_t writeaddress;
    sint16_t writelength;
    sint16_t writesize;
    uint8_t  *bufsrc;
    uint8_t  *bufdst;
    sint16_t bytelen;
    sint16_t copylength;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* skipping */
    //usbd480fb_update_screen(dev); /* page0 */
    //usbd480fb_update_screen(dev); /* page1 */
    //return;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    if (is_update_line == False)
    {
        printk("usbd480fb: update_line pos %d len %d\n", pos, len);
        //is_update_line = True;
    }

    usb_buffer_size = 1024 * 64;

    usbd480fb_set_stream_decoder(dev, STREAM_DECODER_ENABLED);

    wraplength = dev->width - 1;                 // disables wrap

    dev->cmd_buf[0] = 0x43;                      // wrap length
    dev->cmd_buf[1] = 0x5B;
    dev->cmd_buf[2] = wraplength;
    dev->cmd_buf[3] = wraplength >> 8;

    writeaddress = pos;
    writelength  = len - 1;

    dev->cmd_buf[4]  = 0x41;    // pixel write
    dev->cmd_buf[5]  = 0x5B;
    dev->cmd_buf[6]  = writeaddress;
    dev->cmd_buf[7]  = writeaddress >> 8;
    dev->cmd_buf[8]  = writeaddress >> 16;
    dev->cmd_buf[9]  = writeaddress >> 24;
    dev->cmd_buf[10] = writelength;
    dev->cmd_buf[11] = writelength >> 8;
    dev->cmd_buf[12] = writelength >> 16;
    dev->cmd_buf[13] = writelength >> 24;

    writesize = 14;

    result = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, 2),
                          dev->cmd_buf,
                          writesize,
                          &sentsize,
                          5000
                          );

    bufsrc  = dev->vmem + (dev->area_size / 2); /* endian fix*/
    bufsrc  = dev->vmem;
    bufsrc += pos * 2;
    bufdst  = dev->usb_buf;
    bytelen = len * 2;

    while (bytelen > 0)
    {
        copylength = bytelen;

        if (copylength > usb_buffer_size)
        {
            copylength = usb_buffer_size;
        }

        memcpy(bufdst, bufsrc, copylength);
        endian_fix_16bit(bufsrc, bufdst, copylength);

        bytelen -= copylength;
        bufsrc  += copylength;

        result = usb_bulk_msg(dev->udev,
                              usb_sndbulkpipe(dev->udev, 2),
                              dev->usb_buf,
                              copylength,
                              &sentsize,
                              5000
                              );
    }
}

boolean_t is_update_page = False;
void usbd480fb_update_page
(
    struct usbd480fb *dev,
    const char_t *vbuf,
    uint16_t pageaddr,
    uint16_t pagesize
)
{
    sint16_t result;
    sint16_t sentsize;
    sint16_t wraplength;
    sint16_t writeaddress;
    sint16_t writelength;
    sint16_t writesize;

    if (is_update_page == False)
    {
        printk("usbd480fb: width %d\n", dev->width);
        printk("usbd480fb: update_page %d\n", pageaddr);
        is_update_page = True;
    }

    usbd480fb_set_stream_decoder(dev, STREAM_DECODER_ENABLED);

    wraplength = dev->width - 1;                 // this should disable wrapping?

    dev->cmd_buf[0] = 0x43;                      // wrap length
    dev->cmd_buf[1] = 0x5B;
    dev->cmd_buf[2] = wraplength;
    dev->cmd_buf[3] = wraplength >> 8;

    writeaddress = pageaddr / 2;
    writelength  = pagesize / 2 - 1;

    dev->cmd_buf[4]  = 0x41;    // pixel write
    dev->cmd_buf[5]  = 0x5B;
    dev->cmd_buf[6]  = writeaddress;
    dev->cmd_buf[7]  = writeaddress >> 8;
    dev->cmd_buf[8]  = writeaddress >> 16;
    dev->cmd_buf[9]  = writeaddress >> 24;
    dev->cmd_buf[10] = writelength;
    dev->cmd_buf[11] = writelength >> 8;
    dev->cmd_buf[12] = writelength >> 16;
    dev->cmd_buf[13] = writelength >> 24;

    writesize = 14;

    result = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, 2),
                          dev->cmd_buf,
                          writesize,
                          &sentsize,
                          5000
                          );

    // image data next
    memcpy(dev->usb_buf, dev->vmem + pageaddr, PAGE_SIZE);

    result = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, 2),
                          dev->usb_buf,
                          pagesize,
                          &sentsize,
                          5000
                          );
}

