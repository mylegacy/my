
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

/*
 * USBD480 USB display framebuffer driver
 * working on PowerBook-G4 with fbiterm
 */

/*
 * USBD480-LQ043 is a 480x272 pixel display
 * with 16 bpp RBG565 colors.
 *
 * There are also displays with other resolutions
 * so any specific size should not be assumed.
 *
 * To use this driver you should be running
 * firmware version 0.5 (2009/05/28) or later.
 *
 * Tested with display resolutions 480x272
 * kernel 2.6.26
 *
 *
 * ideas, todo:
 * ============
 *
 *     error handling
 *     backlight support - separate driver?
 *     double buffering
 *     alternatively instead of continuosly updating
 *     wait for an update command from application?
 *     performance optimisation
 *     suspend/resume
 */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define uint32_t  unsigned long
#define boolean_t unsigned char
#define True      1
#define False     0
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define FIXENDIAN            yes
#define SCREEN_WIDTH         480
#define SCREEN_HEIGHT        272
#define lcd_refresh_delay    10
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 * The screen size is set up for the default
 * 8x8 character size of fbcon
 */
#define BYTES_PER_PIXEL    2
#define BITS_PER_PIXEL     (BYTES_PER_PIXEL * 8)
#define ROWLEN             (SCREEN_WIDTH * BYTES_PER_PIXEL)


#define RED_SHIFT          16       
#define GREEN_SHIFT        8
#define BLUE_SHIFT         0

#define AREASIZE           (SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL)

boolean_t is_hwscroll = False;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#define USBD480_INTEPDATASIZE              16  /* 16 bit interface */

#define USBD480_VID                        0x16C0
#define USBD480_PID                        0x08A6

#define USBD480_SET_ADDRESS                0xC0
#define USBD480_SET_FRAME_START_ADDRESS    0xC4
#define USBD480_SET_BRIGHTNESS             0x81
#define USBD480_GET_DEVICE_DETAILS         0x80

#define IOCTL_SET_BRIGHTNESS               0x10
#define IOCTL_GET_DEVICE_DETAILS           0x20


#define USBD480_VIDEOMEMORDER              (get_order(PAGE_ALIGN(dev->vmemsize)))

//#define USBD480_REFRESH_DELAY              1000 / 100 /* about xx fps, less in practice */
#define USBD480_REFRESH_DELAY              500 / 100   /* about xx fps, less in practice */
#define USBD480_REFRESH_JIFFIES            ((USBD480_REFRESH_DELAY * HZ) / 1000)


/*
 * Here we define the default structs fb_fix_screeninfo and fb_var_screeninfo
 * if we don't use modedb. If we do use modedb see xxxfb_init how to use it
 * to get a fb_var_screeninfo. Otherwise define a default var as well.
 */
static struct fb_fix_screeninfo xxxfb_fix =    //__devinitdata = 
{
        .id           = "usbd480fb", /* my */
        .type         = FB_TYPE_PACKED_PIXELS,
//        .visual     = FB_VISUAL_PSEUDOCOLOR, /* used */
        .visual       = FB_VISUAL_TRUECOLOR, /* suggested */
        .xpanstep     = 1,
        .ypanstep     = 1,
        .ywrapstep    = 1,
        .line_length  = SCREEN_WIDTH * BYTES_PER_PIXEL,
        .accel        = FB_ACCEL_NONE,
};

static struct fb_var_screeninfo xxxfb_var __devinitdata = /* new */
{
    .bits_per_pixel = BITS_PER_PIXEL,

    .red          = { RED_SHIFT,    8, 0 },
    .green        = { GREEN_SHIFT,  8, 0 },
    .blue         = { BLUE_SHIFT,   8, 0 },
    .transp       = { 0,            0, 0 },
    .xres         = SCREEN_WIDTH,
    .yres         = SCREEN_HEIGHT,
    .xres_virtual = SCREEN_WIDTH,
    .yres_virtual = SCREEN_HEIGHT,
    .nonstd       =              0,
};

struct xxxfb_drv /* new */
{
    struct fb_info   * info;     /* FB driver info record */
    
//    struct spi_device* spi;      /* Char device structure */
    u16              * scr;
    u32              pseudo_palette[32];   
};

/*
 * Driver
 */
/*new*/ struct xxxfb_drv* main_dev;   /* Allocated dynamically in init function */
    



static struct usb_device_id id_table [] =
{
    { 
    .match_flags        = USB_DEVICE_ID_MATCH_DEVICE |
                          USB_DEVICE_ID_MATCH_INT_CLASS |
                          USB_DEVICE_ID_MATCH_INT_PROTOCOL,
    .idVendor           = USBD480_VID,
    .idProduct          = USBD480_PID,
    .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
    .bInterfaceProtocol = 0x00
    },
    { },
};
MODULE_DEVICE_TABLE(usb, id_table);

static int refresh_delay = lcd_refresh_delay; /* is it used ? where ? */
module_param(refresh_delay, int, 0);
MODULE_PARM_DESC(refresh_delay, "Delay between display refreshes");

struct usbd480
{
    struct usb_device       *udev;
    struct fb_info          *fbinfo;
    struct delayed_work     work;
    struct workqueue_struct *wq;

    unsigned char           *vmem;
    unsigned long           vmemsize;
    unsigned long           vmem_phys;
    unsigned int            disp_page;
    unsigned char           brightness;
    unsigned int            width;
    unsigned int            height;
    char                    device_name[20];
};

static int usbd480_get_device_details(struct usbd480 *dev)
{
    // TODO: return value handling

    int           result;
    unsigned char *buffer;

    buffer = kmalloc(64, GFP_KERNEL);
    if (!buffer)
    {
        dev_err(&dev->udev->dev, "out of memory\n");
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
        dev_dbg(&dev->udev->dev, "result = %d\n", result);
    }

    dev->width  = (unsigned char) buffer[20] | ((unsigned char) buffer[21] << 8);
    dev->height = (unsigned char) buffer[22] | ((unsigned char) buffer[23] << 8);
    strncpy(dev->device_name, buffer, 20);
    kfree(buffer);

    return 0;
}

static int usbd480_set_brightness(struct usbd480 *dev, unsigned int brightness)
{
    // TODO: return value handling, check valid dev?

    int result;

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
    {
        dev_dbg(&dev->udev->dev, "result = %d\n", result);
    }
    return 0;
}

static int usbd480_set_address(struct usbd480 *dev, unsigned int addr)
{
    // TODO: return value handling

    int result;

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
    {
        dev_dbg(&dev->udev->dev, "result = %d\n", result);
    }
    return 0;
}

static int usbd480_set_frame_start_address(struct usbd480 *dev, unsigned int addr)
{
    // TODO: return value handling

    int result;

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
    {
        dev_dbg(&dev->udev->dev, "result = %d\n", result);
    }
    return 0;
}

static ssize_t show_brightness(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->brightness);
}

static ssize_t set_brightness(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct usb_interface *intf      = to_usb_interface(dev);
    struct usbd480       *d         = usb_get_intfdata(intf);
    int                  brightness = simple_strtoul(buf, NULL, 10);

    d->brightness = brightness;

    usbd480_set_brightness(d, brightness);

    return count;
}

static ssize_t show_width(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->width);
}

static ssize_t show_height(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%d\n", d->height);
}

static ssize_t show_name(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct usb_interface *intf = to_usb_interface(dev);
    struct usbd480       *d    = usb_get_intfdata(intf);

    return sprintf(buf, "%s\n", d->device_name);
}

static DEVICE_ATTR(brightness, S_IWUGO | S_IRUGO, show_brightness, set_brightness);
static DEVICE_ATTR(width, S_IRUGO, show_width, NULL);
static DEVICE_ATTR(height, S_IRUGO, show_height, NULL);
static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
int xxxfb_init(void);

/*
 *      xxxfb_open - Optional function. Called when the framebuffer is
 *                   first accessed.
 *      @info: frame buffer structure that represents a single frame buffer
 *      @user: tell us if the userland (value=1) or the console is accessing
 *             the framebuffer.
 *
 *      This function is the first function called in the framebuffer api.
 *      Usually you don't need to provide this function. The case where it
 *      is used is to change from a text mode hardware state to a graphics
 *      mode state.
 *
 *      Returns negative errno on error, or zero on success.
 */
static int xxxfb_open(struct fb_info *info, int user)
{
    return 0;
}

/*
 *      xxxfb_release - Optional function. Called when the framebuffer
 *                      device is closed.
 *      @info: frame buffer structure that represents a single frame buffer
 *      @user: tell us if the userland (value=1) or the console is accessing
 *             the framebuffer.
 *
 *      Thus function is called when we close /dev/fb or the framebuffer
 *      console system is released. Usually you don't need this function.
 *      The case where it is usually used is to go from a graphics state
 *      to a text mode state.
 *
 *      Returns negative errno on error, or zero on success.
 */
static int xxxfb_release(struct fb_info *info, int user)
{
    return 0;
}

static int xxxfb_ioctl(struct fb_info *info, u_int cmd, u_long arg)
{
    return 0;
}

static void lcd_initScreen()
{
}

static void lcd_clear()
{
}

static inline unsigned int chan_to_field
(
unsigned int chan,
const struct fb_bitfield *bf
)
{                            
    chan  &= 0xffff;
    chan >>= 16 - bf->length;
    return chan << bf->offset;
}   

static int xxxfb_setcolreg
(
unsigned int regno, 
unsigned int red,
unsigned int green, 
unsigned int blue,   
unsigned int transp, 
struct fb_info *info
)
{
    unsigned int val;
    unsigned int color;
    u32          *pal;
    int          ret = 1;

    if (info->var.grayscale)
       {
        color = (19595 * red + 38470 * green + 7471 * blue) >> 16;
        red = color;
        green = color;
        blue = color; 
       }
    switch (info->fix.visual)
    {
    case FB_VISUAL_TRUECOLOR:
        if (regno < 16)
        {
            pal = info->pseudo_palette;
                             
            val  = chan_to_field(red, &info->var.red);
            val |= chan_to_field(green, &info->var.green);
            val |= chan_to_field(blue, &info->var.blue);
    
            pal[regno] = val;
            ret        = 0; 
        }
        break;
    default:
        break;
    }
    return ret;
}


static void xxxfb_delay(int n)
{
    mdelay(n);
}
static void update_Screen
(
    struct fb_info* info,
    unsigned int xPos,
    unsigned int yPos,
    unsigned int w,
    unsigned int h
)
{
    int x, y;
    u8  *vmem = info->screen_base;
    u8  *src;
    u16 dst;

    if (xPos > SCREEN_WIDTH || yPos > SCREEN_HEIGHT || (xPos + w) > SCREEN_WIDTH || (yPos + h) > SCREEN_HEIGHT)
    {
        printk(KERN_WARNING "updateNokiaScreen: Invalid coordinate. (%d,%d)\n", xPos, yPos);
        return;
    }
}

static ssize_t xxxfb_read
(
    struct fb_info *info,
    char *buf,
    size_t count,
    loff_t * ppos
)
{
    unsigned long p = *ppos;
    unsigned int  fb_mem_len;
    char          *base_addr;
        
    fb_mem_len = SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL;
    if (p >= fb_mem_len)
    {
        return 0;
    }
    if (count >= fb_mem_len)
    {
        count = fb_mem_len;
    }
    if (count + p > fb_mem_len)
    {
        count = fb_mem_len - p;
    }

    if (count > 0)
    {
        base_addr = info->screen_base;
        count    -= copy_to_user(buf, base_addr + p, count);
        if (count == 0)
        {
            return -EFAULT;
        }
        *ppos += count;
    }
    return count;
}

static ssize_t xxxfb_write
(
    struct fb_info *info,
    const char *buf,
    size_t count,
    loff_t * ppos
)
{
    //struct xxxfb_drv *par = info->par;
    unsigned long p          = *ppos;
    unsigned int  fb_mem_len = SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL;
    int           err;        char *base_addr;

    if (p > fb_mem_len)
    {
        return -ENOSPC;
    }
    if (count >= fb_mem_len)
    {
        count = fb_mem_len;
    }
    err = 0;
 
    if (count + p > fb_mem_len)
    {
        count = fb_mem_len - p;
        err   = -ENOSPC;
    }
    if (count > 0)
    if (p > fb_mem_len)
    {
        return -ENOSPC;
    }
    if (count >= fb_mem_len)
    {
        count = fb_mem_len;
    }
    err = 0;
 
    if (count + p > fb_mem_len)
    {
        count = fb_mem_len - p;
        err   = -ENOSPC;
    }
    if (count > 0)
    if (p > fb_mem_len)
    {
        return -ENOSPC;
    }
    if (count >= fb_mem_len)
    {
        count = fb_mem_len;
    }
    err = 0;
 
    if (count + p > fb_mem_len)
    {
        count = fb_mem_len - p;
        err   = -ENOSPC;
    }
    if (count > 0)
    {
        base_addr = info->screen_base;
        count    -= copy_from_user(base_addr + p, buf, count);
        *ppos    += count;
        err       = -EFAULT;
    }
    if (count > 0)
    {
        // Always updates whole lines.
        update_Screen(info, 0, p / ROWLEN, SCREEN_WIDTH, (count + ROWLEN - 1) / ROWLEN);
        return count;
    }
    return err;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef union
{
    uint32_t word;
    char     byte[4];
} uint32_u_t;

static void endian_fix_16bit(uint32_t source[], uint32_t target[], uint32_t size)
{
    uint32_t   i;
    uint32_u_t value1, value2;

    for (i = 0; i < size / 4; i++)
    {
        /*
         * target[i] = source[i]
         */
        value1.word    = source[i];
        value2.byte[0] = value1.byte[1];
        value2.byte[1] = value1.byte[0];
        value2.byte[2] = value1.byte[3];
        value2.byte[3] = value1.byte[2];

        //value3.byte[0] = 0;
        //value2.byte[1] = 0;
        //value2.byte[2] = 0;
        //value2.byte[3] = 0;

        target[i] = value2.word;
    }
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void usbd480fb_work(struct work_struct *work)
{
    struct usbd480 *d;
    int            result;
    int            sentsize;
    int            writeaddr;
    int            showaddr;
    long           vmemsize_new;

    d = container_of(work, struct usbd480, work.work);

    vmemsize_new = d->width * d->height * 2;

    if (d->disp_page == 0)
    {
        writeaddr    = 0;
        showaddr     = 0;
        d->disp_page = 1;
    }
    else
    {
        writeaddr    = vmemsize_new;
        showaddr     = vmemsize_new;
        d->disp_page = 0;
    }

    usbd480_set_address(d, writeaddr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 * Synopsis
 * int usb_bulk_msg
 *     (
 *       struct usb_device * usb_dev,
 *       unsigned int pipe,
 *       void * data,
 *       int len,
 *       int * actual_length,
 *       int timeout
 *     );
 * Arguments
 * usb_dev
 * pointer to the usb device to send the message to
 *
 * pipe
 * endpoint "pipe" to send the message to
 *
 * data
 * pointer to the data to send
 *
 * len
 * length in bytes of the data to send
 *
 * actual_length
 * pointer to a location to put the actual length transferred in bytes
 *
 * timeout
 * time to wait for the message to complete before timing out
 * if 0 the wait is forever
 *
 * ans
 * If successful, it returns 0, othwise a negative error number.
 */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef __BIG_ENDIAN
#warning "big endian"
#endif

/* danilo patch */
//#undef FIXENDIAN

#ifdef FIXENDIAN
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* lcd is little endian, host is big endian */
#warning "using fixendian for big endian"

    endian_fix_16bit(d->vmem, d->vmem + vmemsize_new, vmemsize_new);

    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          d->vmem + vmemsize_new,
                          vmemsize_new,
                          &sentsize,
                          5000);

#else
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* lcd is little endian, host is little endian */
    result = usb_bulk_msg(d->udev,
                          usb_sndbulkpipe(d->udev, 2),
                          d->vmem,
                          d->vmemsize,
                          &sentsize,
                          5000);
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#endif

    usbd480_set_frame_start_address(d, showaddr);

    queue_delayed_work(d->wq, &d->work, USBD480_REFRESH_JIFFIES);
}

static struct fb_ops usbd480fb_ops =
{
    .owner        = THIS_MODULE,
//    .fb_read      = fb_sys_read,
//    .fb_write     = fb_sys_write,
/* new */    .fb_read      = xxxfb_read,
/* new */    .fb_write     = xxxfb_write,
    .fb_fillrect  = sys_fillrect,
    .fb_copyarea  = sys_copyarea,
    .fb_imageblit = sys_imageblit,
    .fb_open      = xxxfb_open,    /* my */
    .fb_release   = xxxfb_release, /* my */
    .fb_ioctl     = xxxfb_ioctl,   /* my */
/* new */ .fb_setcolreg = xxxfb_setcolreg,
// maybe .fb_blank     = fb_blank,
};



static int usbd480_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev  = interface_to_usbdev(interface);
    struct usbd480    *dev   = NULL;
    int               retval = -ENOMEM;
    signed long       size;
    unsigned long     addr;
    struct fb_info    *info;

    dev = kzalloc(sizeof(struct usbd480), GFP_KERNEL);
    if (dev == NULL)
    {
        retval = -ENOMEM;
        dev_err(&interface->dev, "Out of memory\n");
        goto error_dev;
    }

    dev->udev = usb_get_dev(udev);
    usb_set_intfdata(interface, dev);

/*
        retval = usb_register_dev(interface, &usbd480_class);
        if (retval)
        {
                err("Not able to get a minor for this device.");
                return -ENOMEM;
        }
 */

    retval = device_create_file(&interface->dev, &dev_attr_brightness);
    if (retval)
    {
        goto error_dev_attr;
    }
    retval = device_create_file(&interface->dev, &dev_attr_width);
    if (retval)
    {
        goto error_dev_attr;
    }
    retval = device_create_file(&interface->dev, &dev_attr_height);
    if (retval)
    {
        goto error_dev_attr;
    }
    retval = device_create_file(&interface->dev, &dev_attr_name);
    if (retval)
    {
        goto error_dev_attr;
    }

    dev_info(&interface->dev, "USBD480 attached\n");

    usbd480_get_device_details(dev);
    /*
     * dev->vmemsize = dev->width*dev->height*2;
     */
    dev->vmemsize = dev->width * dev->height * 2 * 2;
    dev->vmem     = NULL;

    if (!(dev->vmem = (void *) __get_free_pages(GFP_KERNEL, USBD480_VIDEOMEMORDER)))
    {
        printk(KERN_ERR ": can't allocate vmem buffer");
        retval = -ENOMEM;
        goto error_vmem;
    }

    size = PAGE_SIZE * (1 << USBD480_VIDEOMEMORDER);
    addr = (unsigned long) dev->vmem;

    while (size > 0)
    {
        SetPageReserved(virt_to_page((void *) addr));
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    dev->vmem_phys = virt_to_phys(dev->vmem);
    memset(dev->vmem, 0, dev->vmemsize);

    info = framebuffer_alloc(0, NULL);
    if (!info)
    {
        printk("error: framebuffer_alloc\n");
        goto error_fballoc;
    }

/* new */    lcd_initScreen();
/* new */    lcd_clear();



#ifdef __BIG_ENDIAN
    /* danilo patch */
    info->flags = FBINFO_FOREIGN_ENDIAN | FBINFO_FLAG_DEFAULT;
#endif


/* . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . */
    /*
     * Here we set the screen_base to the virtual memory address
     * for the framebuffer. Usually we obtain the resource address
     * from the bus layer and then translate it to virtual memory
     * space via ioremap. Consult ioport.h.
     */
    //info->screen_base = framebuffer_virtual_memory;
                         /* framebuffer_virtual_memory */
    info->screen_base     = (char __iomem *) dev->vmem;
    info->screen_size     = dev->vmemsize;

    info->var             = xxxfb_var;
    info->fix = xxxfb_fix; 
                             /* 
                              * this will be the only time xxxfb_fix will be
                              * used, so mark it as __devinitdata
                              */
/* . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . */


    //info->fbops = &xxxfb_ops;
    info->fbops           = &usbd480fb_ops;

    info->fix.type        = FB_TYPE_PACKED_PIXELS;
    info->fix.visual      = FB_VISUAL_TRUECOLOR;
                         /* 
                          * FB_VISUAL_PSEUDOCOLOR
                          * FB_VISUAL_DIRECTCOLOR 
                          */

    if ( is_hwscroll == True )
    {
        printk(KERN_WARNING " hw scroll mode\n");
        info->flags |= (FBINFO_HWACCEL_COPYAREA | FBINFO_HWACCEL_FILLRECT);
    }
    else
    {
        printk(KERN_WARNING " sw scroll mode\n");
    }

    info->fix.xpanstep    = 0;
    info->fix.ypanstep    = 0;
    info->fix.ywrapstep   = 0;
    info->fix.line_length = dev->width * 16 / 8;
    info->fix.accel       = FB_ACCEL_NONE;

    info->fix.smem_start = (unsigned long) dev->vmem_phys;
    info->fix.smem_len   = dev->vmemsize;

    //info->var.xres =      dev->width;
    //info->var.yres =      dev->height;
    info->var.xres = SCREEN_WIDTH;
    info->var.yres = SCREEN_HEIGHT;
    //info->var.xres_virtual =  dev->width;
    //info->var.yres_virtual =  dev->height; /*8738*/
    info->var.xres_virtual   = SCREEN_WIDTH;
    info->var.yres_virtual   = SCREEN_HEIGHT;         
    info->var.bits_per_pixel = 16;
/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usbd480: x=%d,y=%d\n", dev->width, dev->height);
    /*
     * myhere
     * Tested with display resolutions 480x272
     */
    info->var.red.offset   = 11;
    info->var.red.length   = 5;
    info->var.green.offset = 5;
    info->var.green.length = 6;
    info->var.blue.offset  = 0;
    info->var.blue.length  = 5;
/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    info->var.left_margin  = 0;
    info->var.right_margin = 0;
    info->var.upper_margin = 0;
    info->var.lower_margin = 0;
    info->var.vmode        = FB_VMODE_NONINTERLACED;

    info->par            = NULL;
    info->flags          = FBINFO_FLAG_DEFAULT; /*FBINFO_HWACCEL_YPAN */


    //info->pseudo_palette = NULL;
    //info->pseudo_palette = pseudo_palette; 
                             /* 
                              * The pseudopalette is an 16-member array
                              */
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

    dev->wq = create_singlethread_workqueue("usbd480fb");     //TODO: create unique names?
    if (!dev->wq)
    {
        err("Could not create work queue\n");
        retval = -ENOMEM;
        goto error_wq;
    }

    INIT_DELAYED_WORK(&dev->work, usbd480fb_work);
    queue_delayed_work(dev->wq, &dev->work, USBD480_REFRESH_JIFFIES * 4);

    printk(KERN_INFO
           "fb%d: USBD480 framebuffer device, using %ldK of memory, refresh %d Hz\n",
           info->node, dev->vmemsize >> 10, USBD480_REFRESH_DELAY);

    return 0;

 error_wq:
    unregister_framebuffer(info);
 error_fbreg:
    fb_dealloc_cmap(&info->cmap);
 error_fballoccmap:
    if (info->pseudo_palette)
    {
        kfree(info->pseudo_palette);
    }
 error_fbpseudopal:
    framebuffer_release(info);
 error_fballoc:
    size = PAGE_SIZE * (1 << USBD480_VIDEOMEMORDER);
    addr = (unsigned long) dev->vmem;
    while (size > 0)
    {
        ClearPageReserved(virt_to_page((void *) addr));
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    free_pages((unsigned long) dev->vmem, USBD480_VIDEOMEMORDER);
 error_vmem:
 error_dev_attr:
    device_remove_file(&interface->dev, &dev_attr_brightness);
    device_remove_file(&interface->dev, &dev_attr_width);
    device_remove_file(&interface->dev, &dev_attr_height);
    device_remove_file(&interface->dev, &dev_attr_name);
 error_dev:
    usb_set_intfdata(interface, NULL);
    if (dev)
    {
        kfree(dev);
    }
    printk(KERN_INFO "usbd480fb: error probe\n");
    return retval;
}

static void usbd480_disconnect(struct usb_interface *interface)
{
    struct usbd480 *dev;
    struct fb_info *info;
    signed long    size;
    unsigned long  addr;

    dev = usb_get_intfdata(interface);

    cancel_delayed_work_sync(&dev->work);
    flush_workqueue(dev->wq);
    destroy_workqueue(dev->wq);

    //usb_deregister_dev(interface, &usbd480_class);

    device_remove_file(&interface->dev, &dev_attr_brightness);
    device_remove_file(&interface->dev, &dev_attr_width);
    device_remove_file(&interface->dev, &dev_attr_height);
    device_remove_file(&interface->dev, &dev_attr_name);

    info = dev->fbinfo;
    if (info)
    {
        unregister_framebuffer(info);
        framebuffer_release(info);
        size = PAGE_SIZE * (1 << USBD480_VIDEOMEMORDER);
        addr = (unsigned long) dev->vmem;
        while (size > 0)
        {
            ClearPageReserved(virt_to_page((void *) addr));
            addr += PAGE_SIZE;
            size -= PAGE_SIZE;
        }
        free_pages((unsigned long) dev->vmem, USBD480_VIDEOMEMORDER);
    }

    usb_set_intfdata(interface, NULL);
    usb_put_dev(dev->udev);
    kfree(dev);
    dev_info(&interface->dev, "USBD480 disconnected\n");
}

static struct usb_driver usbd480_driver =
{
    .name       = "usbd480fb",
    .probe      = usbd480_probe,
    .disconnect = usbd480_disconnect,
    .id_table   = id_table,
};

static int __init usbd480_init(void)
{
    int retval = 0;

    retval = usb_register(&usbd480_driver);
    if (retval)
    {
        err("usb_register failed. Error number %d", retval);
    }
    return retval;
}

static void __exit usbd480_exit(void)
{
    usb_deregister(&usbd480_driver);
}

module_init(usbd480_init);
module_exit(usbd480_exit);

MODULE_AUTHOR("kernel");
MODULE_DESCRIPTION("USBD480 framebuffer driver");
MODULE_LICENSE("GPL");

