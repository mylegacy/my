#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>

#include "types.inc"
#include "config.inc"
#include "options.inc"

struct usbd480
{
    struct usb_device       *udev;
    struct fb_info          *fbinfo;
    struct delayed_work     work;
    struct workqueue_struct *wq;
    struct kref             kref;

    uint8_t                 *vmem;
    uint32_t                vmemsize;
    uint32_t                vmem_phys;
    uint16_t                disp_page;
    uint8_t                 brightness;
    uint16_t                width;
    uint16_t                height;
    uint16_t                byte_per_pixel; /* new */
    uint16_t                area_size;      /* new */
    char_t                  device_name[20];
    uint8_t                 *cmd_buf;
    uint8_t                 *usb_buf;
    sint16_t                client_count;
};

static sint16_t usbd480_get_device_details(struct usbd480 *dev)
{
    // TODO: return value handling

    sint16_t result;
    uint8_t  *buffer;

    printk("usbd480fb: geting details ... ");

    buffer = kmalloc(64, GFP_KERNEL);
    if (!buffer)
    {
        //dev_err(&dev->udev->dev, "out of memory\n");
        printk("failed, out of memory\n");
        return 0;
    }

    result = usb_control_msg(dev->udev,
                             usb_rcvctrlpipe(dev->udev, 0),
                             USBD480_GET_DEVICE_DETAILS,
                             USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
                             0,
                             0,
                             buffer,
                             64,
                             1000);
    if (result)
    {
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

static sint16_t usbd480_set_brightness(struct usbd480 *dev, uint16_t brightness)
{
    // TODO: return value handling, check valid dev?

    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             USBD480_SET_BRIGHTNESS,
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

static sint16_t usbd480_set_address(struct usbd480 *dev, uint16_t addr)
{
    // TODO: return value handling
    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             USBD480_SET_ADDRESS,
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

static sint16_t usbd480_set_frame_start_address(struct usbd480 *dev, uint16_t addr)
{
    // TODO: return value handling
    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             USBD480_SET_FRAME_START_ADDRESS,
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

static sint16_t usbd480_set_stream_decoder(struct usbd480 *dev, uint16_t sd)
{
    // TODO: return value handling, check valid dev?

    sint16_t result;

    result = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             USBD480_SET_STREAM_DECODER,
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

boolean_t is_update_rect = False;

void usbd480_update_rect
(
    struct usbd480 *dev,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height
)
{
    //TODO: lots of optimizations

    /*
       This implementation is limited to using a 64K preallocated usb buffer.
       Also this implementation just discards the extra data if it doesn't fit
       in single 64K buffer.
       TODO: if 64K isn't enough, use the buffer several times
       TODO: Switch to using urbs instead of usb_bulk_msg
     */
    const sint16_t usb_buffer_size = 1024 * 64;

    struct usbd480 *d = dev;
    sint16_t       result;
    sint16_t       sentsize;
    sint16_t       wraplength;
    sint16_t       writeaddress;
    sint16_t       writelength;
    sint16_t       writesize;
    sint16_t       ypos;
    uint8_t        *bufsrc;
    uint8_t        *bufdst;
    sint16_t       bufsize;
    sint16_t       copylength;

    if (is_update_rect == False)
       {
       printk("usbd480fb: update_rect using usb_buffer of %d bytes\n", usb_buffer_size);
       printk("usbd480fb: update_rect %d, %d, %d, %d size %d\n", x, y, width, height, width * height);
       is_update_rect = True;
       }

    usbd480_set_stream_decoder(d, 0x06);     // enable stream decoder - shouldn't be necessary to do this more than once while initializing display

    wraplength = width - 1;

    d->cmd_buf[0] = 0x43;     // wrap length
    d->cmd_buf[1] = 0x5B;
    d->cmd_buf[2] = wraplength;
    d->cmd_buf[3] = wraplength >> 8;

    writeaddress = ((y * d->width) + x);
    writelength  = width * height - 1;

    if (writelength > (usb_buffer_size / 2) - 1)
    {
        writelength = (usb_buffer_size / 2) - 1;
    }
    d->cmd_buf[4]  = 0x41;    // pixel write
    d->cmd_buf[5]  = 0x5B;
    d->cmd_buf[6]  = writeaddress;
    d->cmd_buf[7]  = writeaddress >> 8;
    d->cmd_buf[8]  = writeaddress >> 16;
    d->cmd_buf[9]  = writeaddress >> 24;
    d->cmd_buf[10] = writelength;
    d->cmd_buf[11] = writelength >> 8;
    d->cmd_buf[12] = writelength >> 16;
    d->cmd_buf[13] = writelength >> 24;

    writesize = 14;

    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          d->cmd_buf,
                          writesize,
                          &sentsize,
                          5000
                          );

    writesize = width * height * BYTES_PER_PIXEL;
    if (writesize > usb_buffer_size)
    {
        writesize = usb_buffer_size;
    }

    bufsize = usb_buffer_size;

    copylength = width * 2;
    bufsrc     = d->vmem;
    bufsrc    += (y * d->width + x) * 2;

    bufdst = d->usb_buf;

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
        bufsrc  += d->fbinfo->fix.line_length;
    }

    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          d->usb_buf,
                          writesize,
                          &sentsize,
                          5000);
}

boolean_t is_update_line = False;
void usbd480_update_line
(
    struct usbd480 *dev,
    uint16_t pos,
    uint16_t len
)
{
    //TODO: lots of optimizations

    /*
       This implementation is limited to using a 64K preallocated usb buffer.
       TODO: Switch to using urbs instead of usb_bulk_msg
     */
    sint16_t       usb_buffer_size;
    struct usbd480 *d;
    sint16_t       result;
    sint16_t       sentsize;
    sint16_t       wraplength;
    sint16_t       writeaddress;
    sint16_t       writelength;
    sint16_t       writesize;
    uint8_t        *bufsrc;
    uint8_t        *bufdst;
    sint16_t       bytelen;
    sint16_t       copylength;

    if (is_update_line == False)
       {
       printk("usbd480fb: update_line pos %d len %d\n", pos, len);
       is_update_line = True;
       }

    usb_buffer_size = 1024 * 64;
    d               = dev;

    usbd480_set_stream_decoder(d, 0x06);     // enable stream decoder - shouldn't be necessary to do this more than once while initializing display

    wraplength = d->width - 1;               // disables wrap

    d->cmd_buf[0] = 0x43;                    // wrap length
    d->cmd_buf[1] = 0x5B;
    d->cmd_buf[2] = wraplength;
    d->cmd_buf[3] = wraplength >> 8;

    writeaddress = pos;
    writelength  = len - 1;

    d->cmd_buf[4]  = 0x41;    // pixel write
    d->cmd_buf[5]  = 0x5B;
    d->cmd_buf[6]  = writeaddress;
    d->cmd_buf[7]  = writeaddress >> 8;
    d->cmd_buf[8]  = writeaddress >> 16;
    d->cmd_buf[9]  = writeaddress >> 24;
    d->cmd_buf[10] = writelength;
    d->cmd_buf[11] = writelength >> 8;
    d->cmd_buf[12] = writelength >> 16;
    d->cmd_buf[13] = writelength >> 24;

    writesize = 14;

    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          d->cmd_buf,
                          writesize,
                          &sentsize,
                          5000);


    bufsrc  = d->vmem + (d->area_size / 2);
    bufsrc += pos * 2;
    bufdst  = d->usb_buf;
    bytelen = len * 2;

    while (bytelen > 0)
    {
        copylength = bytelen;

        if (copylength > usb_buffer_size)
        {
            copylength = usb_buffer_size;
        }

        memcpy(bufdst, bufsrc, copylength);

        bytelen -= copylength;
        bufsrc  += copylength;

        result = usb_bulk_msg(d->udev,
                              usb_sndbulkpipe(d->udev, 2),
                              d->usb_buf,
                              copylength,
                              &sentsize,
                              5000);
    }
}


boolean_t is_update_page = False;

void usbd480_update_page
(
    struct usbd480 *dev,
    const char_t *vbuf,
    uint16_t pageaddr,
    uint16_t pagesize
)
{
    //TODO: lots of optimizations


    struct usbd480 *d;
    sint16_t       result;
    sint16_t       sentsize;
    sint16_t       wraplength;
    sint16_t       writeaddress;
    sint16_t       writelength;
    sint16_t       writesize;

    d = dev;

    if ( is_update_page == False)
       {
       printk("usbd480fb: width %d\n", d->width);
       printk("usbd480fb: update_page %d\n", pageaddr);
       is_update_page = True;
       }

    usbd480_set_stream_decoder(d, 0x06);     // enable stream decoder - shouldn't be necessary to do this more than once while initializing display

    wraplength = d->width - 1;               // this should disable wrapping?

    d->cmd_buf[0] = 0x43;                    // wrap length
    d->cmd_buf[1] = 0x5B;
    d->cmd_buf[2] = wraplength;
    d->cmd_buf[3] = wraplength >> 8;

    writeaddress = pageaddr / 2;
    writelength  = pagesize / 2 - 1;

    d->cmd_buf[4]  = 0x41;    // pixel write
    d->cmd_buf[5]  = 0x5B;
    d->cmd_buf[6]  = writeaddress;
    d->cmd_buf[7]  = writeaddress >> 8;
    d->cmd_buf[8]  = writeaddress >> 16;
    d->cmd_buf[9]  = writeaddress >> 24;
    d->cmd_buf[10] = writelength;
    d->cmd_buf[11] = writelength >> 8;
    d->cmd_buf[12] = writelength >> 16;
    d->cmd_buf[13] = writelength >> 24;

    writesize = 14;

    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          d->cmd_buf,
                          writesize,
                          &sentsize,
                          5000);


    // image data next
    memcpy(d->usb_buf, d->vmem + pageaddr, PAGE_SIZE);

    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          d->usb_buf,
                          pagesize,
                          &sentsize,
                          5000);
}

static ssize_t show_brightness(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->brightness);
}

static ssize_t set_brightness(struct device *dev, struct device_attribute *attr, const char_t *buf, size_t count)
{
    struct usb_interface *intf      = to_usb_interface(dev);
    struct usbd480       *d         = usb_get_intfdata(intf);
    sint16_t             brightness = simple_strtoul(buf, NULL, 10);

    d->brightness = brightness;

    usbd480_set_brightness(d, brightness);

    return count;
}

static ssize_t show_width(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->width);
}

static ssize_t show_height(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->height);
}

static ssize_t show_name(struct device *dev, struct device_attribute *attr, char_t *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%s\n", d->device_name);
}

static DEVICE_ATTR(brightness, S_IWUGO | S_IRUGO, show_brightness, set_brightness);
static DEVICE_ATTR(width, S_IRUGO, show_width, NULL);
static DEVICE_ATTR(height, S_IRUGO, show_height, NULL);
static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

#ifdef USBD480_FULLSCREEN_UPDATE

/*
 * on big endian it needs endian fix up
 */
#include "fixendian.inc"

static void usbd480fb_work(struct work_struct *work)
{
    struct usbd480 *d;

#ifdef USBD480_GETFREEPAGES
    uint32_t vmem_size_new;
    sint16_t result;
    sint16_t sentsize;
    sint16_t writeaddr;
    sint16_t showaddr;

    d             = container_of(work, struct usbd480, work.work);
    vmem_size_new = d->area_size / 2; //AREASIZE / 2; /* half the whole */

    #ifdef __BIG_ENDIAN
    #warning "big endian"
    #endif

    if (d->disp_page == 0)
    {
        writeaddr    = 0;
        showaddr     = 0;
        d->disp_page = 1;
    }
    else
    {
        writeaddr    = vmem_size_new;
        showaddr     = vmem_size_new;
        d->disp_page = 0;
    }

    usbd480_set_stream_decoder(d, 0x00);     // disable stream decoder

    usbd480_set_address(d, writeaddr);

    endian_fix_16bit((uint32_t*) d->vmem, (uint32_t*) (d->vmem + vmem_size_new), (uint32_t) vmem_size_new);

    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          (d->vmem + vmem_size_new),
                          vmem_size_new,
                          &sentsize,
                          5000
                          );

    usbd480_set_frame_start_address(d, showaddr);
#endif

#ifdef USBD480_VMALLOC
    /* no double buffering implemented here */

    d = container_of(work, struct usbd480, work.work);
    usbd480_update_line(d, 0, d->width * d->height);
#endif
    queue_delayed_work(d->wq, &d->work, USBD480_REFRESH_JIFFIES);
}
#endif


#ifdef USBD480_DEFIO
/* broadsheetfb is one example for defio */
static void usbd480fb_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
    struct page           *cur;
    struct fb_deferred_io *fbdefio = info->fbdefio;

    list_for_each_entry(cur, &fbdefio->pagelist, lru)
    {
        usbd480_update_page(info->par, (char_t *) info->fix.smem_start, cur->index << PAGE_SHIFT, PAGE_SIZE);
    }
}

static struct fb_deferred_io usbd480fb_defio =
{
    .delay       = HZ / 20,
    .deferred_io = usbd480fb_deferred_io,
};
#endif


static void usbd480_ops_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
    struct usbd480 *pusbd480fb = info->par;

    sys_copyarea(info, area);

    usbd480_update_rect(pusbd480fb, area->dx, area->dy, area->width, area->height);
}

static void usbd480_ops_imageblit(struct fb_info *info, const struct fb_image *image)
{
    struct usbd480 *d = info->par;

    sys_imageblit(info, image);

    usbd480_update_rect(d, image->dx, image->dy, image->width, image->height);
}

static void usbd480_ops_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    struct usbd480 *pusbd480fb = info->par;

    sys_fillrect(info, rect);

    usbd480_update_rect(pusbd480fb, rect->dx, rect->dy, rect->width, rect->height);
}

static sint16_t usbd480fb_ops_open(struct fb_info *info, sint16_t user)
{
    struct usbd480 *d = info->par;

    d->client_count++;

    //printk("usbd480fb: fb%d: user=%d new client, total %d\n", info->node, user, d->client_count);

    return 0;
}

static sint16_t usbd480fb_ops_release(struct fb_info *info, sint16_t user)
{
    struct usbd480 *d = info->par;

    d->client_count--;

    //printk("usbd480fb: fb%d: user=%d release client, total %d\n", info->node, user, d->client_count);

    return 0;
}

static sint16_t usbd480fb_ops_setcolreg(unsigned regno, unsigned red, unsigned green,
                                        unsigned blue, unsigned transp,
                                        struct fb_info *info)
{
    sint16_t i;
    u32      c;

    printk("usbd480fb: fb%d setcolreg %d r:%d g:%d b:%d\n", info->node, regno, red, green, blue);

    if (regno > 255)
    {
        return 1;
    }

    red   >>= 11;
    green >>= 10;
    blue  >>= 11;

    if (regno < 16)
    {
        c = ((red & 0x3f) << info->var.red.offset) |
            ((green & 0x7f) << info->var.green.offset) |
            ((blue & 0x3f) << info->var.blue.offset);

        ((u32 *) info->pseudo_palette)[regno] = c;
    }

    return 0;
}

#ifdef USBD480_VMALLOC
/* based on udflb */
static sint16_t usbd480fb_ops_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    uint32_t start  = vma->vm_start;
    uint32_t size   = vma->vm_end - vma->vm_start;
    uint32_t offset = vma->vm_pgoff << PAGE_SHIFT;
    uint32_t page, pos;

    if (offset + size > info->fix.smem_len)
        return -EINVAL;

    pos = (uint32_t) info->fix.smem_start + offset;

    printk(KERN_INFO
           "fb%d: mmap() fb addr:%lu size:%lu\n", info->node, pos, size);

    while (size > 0)
    {
        page = vmalloc_to_pfn((void *) pos);
        if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
            return -EAGAIN;

        start += PAGE_SIZE;
        pos   += PAGE_SIZE;
        if (size > PAGE_SIZE)
            size -= PAGE_SIZE;
        else
            size = 0;
    }

    vma->vm_flags |= VM_RESERVED; /* avoibd to swap out this VMA */
    return 0;
}
#endif


static ssize_t usbd480fb_ops_write(struct fb_info *info, const char_t *buf,
                                   size_t count, loff_t * ppos)
{
    struct usbd480 *d = info->par;
    ssize_t        ret;
    uint32_t       p = *ppos;

    ret = fb_sys_write(info, buf, count, ppos);

    if ((p / 2 + count / 2) <= (d->width * d->height))
        usbd480_update_line(info->par, p / 2, count / 2);

    return ret;
}

static struct fb_ops usbd480fb_ops =
{
    .owner        = THIS_MODULE,
    .fb_read      = fb_sys_read,
    .fb_write     = usbd480fb_ops_write,
    .fb_fillrect  = usbd480_ops_fillrect,
    .fb_copyarea  = usbd480_ops_copyarea,
    .fb_imageblit = usbd480_ops_imageblit,
    //.fb_fillrect	= sys_fillrect,
    //.fb_copyarea	= sys_copyarea,
    //.fb_imageblit	= sys_imageblit,
    //.fb_ioctl	= usbd480fb_ioctl,
    //.fb_mmap	= usbd480fb_mmap,
#ifdef USBD480_VMALLOC
    .fb_mmap      = usbd480fb_ops_mmap,
#endif
    //.fb_pan_display = usbd480fb_pan_display,
    .fb_open      = usbd480fb_ops_open,
    .fb_release   = usbd480fb_ops_release,
    .fb_setcolreg = usbd480fb_ops_setcolreg,
};

static sint16_t usbd480_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev  = interface_to_usbdev(interface);
    struct usbd480    *dev   = NULL;
    sint16_t          retval = -ENOMEM;
#ifdef USBD480_GETFREEPAGES
    signed long       size;
    uint32_t          addr;
#endif
    struct fb_info    *info;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#ifdef USBD480_GETFREEPAGES
#endif
    printk("usb480dfb: method getfreepages\n");
#ifdef USBD480_VMALLOC
    printk("usb480dfb: method vmalloc\n");
#endif

#ifdef USBD480_FULLSCREEN_UPDATE
    printk("usb480dfb: method fullscreen_update\n");
#endif

#ifdef USBD480_DEFIO
    printk("usb480dfb: method defio\n");
#endif
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    #ifdef __BIG_ENDIAN
    printk("usbd480fb: host is big-endian, fixendian16 will be used\n");
    #else
    printk("usbd480fb: host is little-endian\n");
    #endif


    dev = kzalloc(sizeof(struct usbd480), GFP_KERNEL);
    if (dev == NULL)
    {
        retval = -ENOMEM;
        dev_err(&interface->dev, "usbd480_probe: failed to alloc\n");
        goto error_dev;
    }

    dev->udev = usb_get_dev(udev);
    usb_set_intfdata(interface, dev);

    retval = device_create_file(&interface->dev, &dev_attr_brightness);
    if (retval)
        goto error_dev_attr;
    retval = device_create_file(&interface->dev, &dev_attr_width);
    if (retval)
        goto error_dev_attr;
    retval = device_create_file(&interface->dev, &dev_attr_height);
    if (retval)
        goto error_dev_attr;
    retval = device_create_file(&interface->dev, &dev_attr_name);
    if (retval)
        goto error_dev_attr;

    dev_info(&interface->dev, "USBD480 attached\n");

    dev->cmd_buf = NULL;
    dev->cmd_buf = kmalloc(64, GFP_KERNEL);
    if (!dev->cmd_buf)
    {
        printk(KERN_ERR ": can't allocate cmd_buf buffer");
        //goto error_cmd_buf;
    }

    dev->usb_buf = NULL;
    dev->usb_buf = kmalloc(1024 * 64, GFP_KERNEL);
    if (!dev->cmd_buf)
    {
        printk(KERN_ERR ": can't allocate usb data buffer");
        //goto error_usb_buf;
    }

    usbd480_get_device_details(dev);
    usbd480_set_frame_start_address(dev, 0);
    usbd480_set_stream_decoder(dev, 0x06);     // enable stream decoder

#ifdef USBD480_DEFIO
    usbd480_set_stream_decoder(dev, 0x06);     // enable stream decoder
#endif


    dev->vmemsize = dev->area_size;
    dev->vmem     = NULL;

#ifdef USBD480_GETFREEPAGES
    if (!(dev->vmem = (void *) __get_free_pages(GFP_KERNEL, USBD480_VIDEOMEMORDER)))
    {
        printk(KERN_ERR ": can't allocate vmem buffer");
        retval = -ENOMEM;
        goto error_vmem;
    }

    size = PAGE_SIZE * (1 << USBD480_VIDEOMEMORDER);
    addr = (uint32_t) dev->vmem;

    while (size > 0)
    {
        SetPageReserved(virt_to_page((void*) addr));
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    dev->vmem_phys = virt_to_phys(dev->vmem);
    memset(dev->vmem, 0, dev->vmemsize);
#endif
#ifdef USBD480_VMALLOC
    /* vmalloc memory not contiguous and not suitable for DMA / USB */
    dev->vmem = vmalloc(dev->vmemsize);
    if (!dev->vmem)
    {
        printk(KERN_ERR ": can't allocate vmem buffer");
        retval = -ENOMEM;
        goto error_vmem;
    }
#endif

#define USBD480_BKG_COLOR 0x20 // f1

    memset(dev->vmem, USBD480_BKG_COLOR, dev->vmemsize);

    info = framebuffer_alloc(0, NULL);
    if (!info)
    {
        printk("error: framebuffer_alloc\n");
        goto error_fballoc;
    }

    //info->screen_base = (char_t __iomem *) dev->vmem;
    info->screen_base = dev->vmem;
    info->screen_size = dev->vmemsize;
    info->fbops       = &usbd480fb_ops;

    info->fix.type        = FB_TYPE_PACKED_PIXELS;
    info->fix.visual      = FB_VISUAL_TRUECOLOR;
    info->fix.xpanstep    = 0;
    info->fix.ypanstep    = 0;     /*1*/
    info->fix.ywrapstep   = 0;
    info->fix.line_length = dev->width * dev->byte_per_pixel; /* new */
                            // 16 / 8; /* tobefixed */
    info->fix.accel       = FB_ACCEL_NONE;
#ifdef USBD480_GETFREEPAGES
    info->fix.smem_start = (uint32_t) dev->vmem_phys;
    info->fix.smem_len   = dev->vmemsize;
#endif
#ifdef USBD480_VMALLOC
    info->fix.smem_start = (uint32_t) dev->vmem;
    info->fix.smem_len   = PAGE_ALIGN(dev->vmemsize);
#endif
    info->var.xres         = dev->width;
    info->var.yres         = dev->height;
    info->var.xres_virtual = dev->width;
    info->var.yres_virtual = dev->height; 

    info->var.bits_per_pixel = dev->byte_per_pixel * 8; //16;
    info->var.red.offset     = 11;
    info->var.red.length     = 5;
    info->var.green.offset   = 5;
    info->var.green.length   = 6;
    info->var.blue.offset    = 0;
    info->var.blue.length    = 5;
    info->var.left_margin    = 0;
    info->var.right_margin   = 0;
    info->var.upper_margin   = 0;
    info->var.lower_margin   = 0;
    info->var.vmode          = FB_VMODE_NONINTERLACED;

    printk("usbd480fb: lcd, %d x %d\n", dev->height, dev->width);
    printk("usbd480fb: lcd, %d byte per pixel\n", dev->byte_per_pixel);
    printk("usbd480fb: lcd, vmode non-interlaced\n");

    info->pseudo_palette = NULL;
    info->par            = dev;
#ifndef FBINFO_VIRTFB
#define FBINFO_VIRTFB    0x0004 /* FB is System RAM, not device. */ /* from fb.h for older kernels that don't have this defined */
#endif
    info->flags = FBINFO_FLAG_DEFAULT
                  | FBINFO_READS_FAST
                  | FBINFO_VIRTFB
                  | FBINFO_HWACCEL_IMAGEBLIT
                  | FBINFO_HWACCEL_FILLRECT
                  | FBINFO_HWACCEL_COPYAREA
                  /*| FBINFO_FOREIGN_ENDIAN*/
                  /*| FBINFO_HWACCEL_YPAN */
    ;

#ifdef USBD480_DEFIO
    info->fbdefio = &usbd480fb_defio;
    fb_deferred_io_init(info);     /* should this be done at some later stage instead? */
#endif
    info->pseudo_palette = kzalloc(sizeof(u32) * 16, GFP_KERNEL);
    if (info->pseudo_palette == NULL)
    {
        retval = -ENOMEM;
        dev_err(&interface->dev, "Failed to allocate pseudo_palette\n");
        goto error_fbpseudopal;
    }

    memset(info->pseudo_palette, 0, sizeof(u32) * 16);

    retval = fb_alloc_cmap(&info->cmap, 256, 0);
    if (retval < 0)
    {
        retval = -ENOMEM;
        dev_err(&interface->dev, "Failed to allocate cmap\n");
        goto error_fballoccmap;
    }

    //printk("phys=%lx p2v=%lx, v=%lx\n", dev->vmem_phys, phys_to_virt(dev->vmem_phys), dev->vmem);

    retval = register_framebuffer(info);
    if (retval < 0)
    {
        printk("error: register_framebuffer \n");
        goto error_fbreg;
    }

    dev->fbinfo    = info;
    dev->disp_page = 0;
#ifdef USBD480_FULLSCREEN_UPDATE
    dev->wq = create_singlethread_workqueue("usbd480fb");     //TODO: create unique names?
    if (!dev->wq)
    {
        err("Could not create work queue\n");
        retval = -ENOMEM;
        goto error_wq;
    }

    INIT_DELAYED_WORK(&dev->work, usbd480fb_work);
    queue_delayed_work(dev->wq, &dev->work, USBD480_REFRESH_JIFFIES * 4);
#endif

    printk("usbd480fb: mapped as framebuffer device /dev/fb%d, using %ldK of memory\n",
           info->node, (dev->vmemsize >> 10));

    return 0;

 error_wq:
    unregister_framebuffer(info);
 error_fbreg:
    fb_dealloc_cmap(&info->cmap);
 error_fballoccmap:
    if (info->pseudo_palette)
        kfree(info->pseudo_palette);
 error_fbpseudopal:
    framebuffer_release(info);
 error_fballoc:
#ifdef USBD480_GETFREEPAGES
    size = PAGE_SIZE * (1 << USBD480_VIDEOMEMORDER);
    addr = (uint32_t) dev->vmem;
    while (size > 0)
    {
        ClearPageReserved(virt_to_page((void*) addr));
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    free_pages((uint32_t) dev->vmem, USBD480_VIDEOMEMORDER);
#endif
#ifdef USBD480_VMALLOC
    vfree(dev->vmem);
#endif
 error_vmem:
    kfree(dev->usb_buf);
    kfree(dev->cmd_buf);
 error_dev_attr:
    device_remove_file(&interface->dev, &dev_attr_brightness);
    device_remove_file(&interface->dev, &dev_attr_width);
    device_remove_file(&interface->dev, &dev_attr_height);
    device_remove_file(&interface->dev, &dev_attr_name);
 error_dev:
    usb_set_intfdata(interface, NULL);
    if (dev)
        kfree(dev);

    printk(KERN_INFO "usbd480fb: error probe\n");
    return retval;
}

static void usbd480_disconnect(struct usb_interface *interface)
{
    struct usbd480 *dev;
    struct fb_info *info;
#ifdef USBD480_GETFREEPAGES
    signed long    size;
    uint32_t       addr;
#endif

    dev = usb_get_intfdata(interface);

#ifdef USBD480_FULLSCREEN_UPDATE
    cancel_delayed_work_sync(&dev->work);
    flush_workqueue(dev->wq);
    destroy_workqueue(dev->wq);
#endif
#ifdef USBD480_DEFIO
    usbd480_set_stream_decoder(dev, 0x00);     // disable stream decoder
#endif

    //usb_deregister_dev(interface, &usbd480_class);

    device_remove_file(&interface->dev, &dev_attr_brightness);
    device_remove_file(&interface->dev, &dev_attr_width);
    device_remove_file(&interface->dev, &dev_attr_height);
    device_remove_file(&interface->dev, &dev_attr_name);

    info = dev->fbinfo;
    if (info)
    {
#ifdef USBD480_DEFIO
        fb_deferred_io_cleanup(info);
#endif
        unregister_framebuffer(info);

        fb_dealloc_cmap(&info->cmap);

        if (info->pseudo_palette)
            kfree(info->pseudo_palette);

        framebuffer_release(info);
#ifdef USBD480_GETFREEPAGES
        size = PAGE_SIZE * (1 << USBD480_VIDEOMEMORDER);
        addr = (uint32_t) dev->vmem;
        while (size > 0)
        {
            ClearPageReserved(virt_to_page((void*) addr));
            addr += PAGE_SIZE;
            size -= PAGE_SIZE;
        }
        free_pages((uint32_t) dev->vmem, USBD480_VIDEOMEMORDER);
#endif
#ifdef USBD480_VMALLOC
        if (dev->vmem)
            vfree(dev->vmem);
#endif
    }

    kfree(dev->usb_buf);
    kfree(dev->cmd_buf);

    usb_set_intfdata(interface, NULL);
    usb_put_dev(dev->udev);
    kfree(dev);
    dev_info(&interface->dev, "USBD480 disconnected\n");

    //kref_put(&dev->kref, );

    //printk(KERN_INFO "usbd480fb: USBD480 disconnected\n");
}

static struct usb_driver usbd480_driver =
{
    .name       = "usbd480fb",
    .probe      = usbd480_probe,
    .disconnect = usbd480_disconnect,
    .id_table   = id_table,
};

static sint16_t __init usbd480_init(void)
{
    sint16_t retval = 0;

    retval = usb_register(&usbd480_driver);
    if (retval)
        err("usb_register failed. Error number %d", retval);
    return retval;
}

static void __exit usbd480_exit(void)
{
    usb_deregister(&usbd480_driver);
}

module_init(usbd480_init);
module_exit(usbd480_exit);

MODULE_AUTHOR("Henri Skippari");
MODULE_DESCRIPTION("USBD480 framebuffer driver");
MODULE_LICENSE("GPL");

