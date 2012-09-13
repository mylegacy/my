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

#include "data.inc"
#include "utils.inc"
#include "colors.inc"
#include "fixendian.inc"
#include "swap16.inc"
#include "update_screen.inc"
#include "primitive_L0.inc"
#include "primitive_L1.inc"

static struct fb_ops usbd480fb_ops =
{
    .owner        = THIS_MODULE,
    .fb_read      = fb_sys_read,
    .fb_write     = usbd480fb_ops_write,
    .fb_fillrect  = usbd480fb_ops_fillrect,
    .fb_copyarea  = usbd480fb_ops_copyarea,
    .fb_imageblit = usbd480fb_ops_imageblit,
    //.fb_fillrect	= sys_fillrect,
    //.fb_copyarea	= sys_copyarea,
    //.fb_imageblit	= sys_imageblit,
    //.fb_ioctl	= usbd480fb_ioctl,
    .fb_mmap      = usbd480fb_ops_mmap,
    //.fb_pan_display = usbd480fb_pan_display,
    .fb_open    = usbd480fb_ops_open,
    .fb_release = usbd480fb_ops_release,
    //.fb_setcolreg = usbd480fb_ops_setcolreg,
};

static sint16_t usbd480fb_probe
(
    struct usb_interface *interface,
    const struct usb_device_id *id
)
{
    struct usb_device *udev;
    struct usbd480fb  *dev;
    sint16_t          retval;
    struct fb_info    *info;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    udev = interface_to_usbdev(interface);
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating dev ... ");
    dev = kzalloc(sizeof(struct usbd480fb), GFP_KERNEL);

    if (dev == NULL)
    {
        retval = -ENOMEM;
        //dev_err(&interface->dev, "usbd480fb_probe: failed to alloc\n");
        printk("failed\n");
        goto error_dev;
    }
    else
    {
        dev->udev = usb_get_dev(udev);
        usb_set_intfdata(interface, dev);
        printk("done\n");
    }
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: creating file ... ");

    goto bypass;
    retval = device_create_file(&interface->dev, &dev_attr_brightness);
    if (retval)
    {
        printk("1 failed\n");
        goto error_dev_attr;
    }
    else
    {
        printk("1 ");
    }
    retval = device_create_file(&interface->dev, &dev_attr_width);
    if (retval)
    {
        printk("2 failed\n");
        goto error_dev_attr;
    }
    else
    {
        printk("2 ");
    }
    retval = device_create_file(&interface->dev, &dev_attr_height);
    if (retval)
    {
        printk("3 failed\n");
        goto error_dev_attr;
    }
    else
    {
        printk("3 ");
    }
    retval = device_create_file(&interface->dev, &dev_attr_name);
    if (retval)
    {
        printk("4 failed\n");
        goto error_dev_attr;
    }
    else
    {
        printk("4 ");
    }

    printk("done\n");
 bypass:

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    dev_info(&interface->dev, "attached\n");
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating cmd_buf ... ");
    dev->cmd_buf = kmalloc(64, GFP_KERNEL);
    if (dev->cmd_buf == NULL)
    {
        printk("failed\n");
        goto error_cmd_buf;
    }
    else
    {
        printk("done\n");
    }
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating usb_buf ... ");
    dev->usb_buf = kmalloc(1024 * 64, GFP_KERNEL);
    if (dev->usb_buf == NULL)
    {
        printk("failed\n");
        goto error_usb_buf;
    }
    else
    {
        printk("done\n");
    }
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    usbd480fb_get_device_details(dev);
    usbd480fb_set_frame_start_address(dev, 0);
    printk("usb480dfb: enabling stream decoder ... ");
    usbd480fb_set_stream_decoder(dev, STREAM_DECODER_ENABLED);
    printk("done\n");
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating videomem ");
    dev->vmemsize = dev->area_size;

    /* vmalloc memory not contiguous and not suitable for DMA / USB */
    printk("using method vmalloc ... ");
    dev->vmem = vmalloc(dev->vmemsize);
    if (!dev->vmem)
    {
        printk("failed\n");
        retval = -ENOMEM;
        goto error_vmem;
    }
    else
    {
        printk("done\n");
    }

    memset(dev->vmem, usbd480fb_BKG_COLOR, dev->vmemsize);
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating private videomem ... ");
    dev->scr = kzalloc(dev->area_size, GFP_KERNEL | GFP_DMA);
    if (dev->scr == NULL)
    {
        printk("failed\n");
    }
    else
    {
        printk("done\n");
    }
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating fbinfo ... ");
    info = framebuffer_alloc(0, NULL);
    if (info == NULL)
    {
        printk("failed\n");
        goto error_fballoc;
    }
    else
    {
        printk("done\n");
    }
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    info->screen_base        = dev->vmem;     /* nature */
    info->screen_base        = (char_t __iomem *) dev->vmem;
    info->screen_size        = dev->vmemsize; /* nature */
    info->fbops              = &usbd480fb_ops;
    info->fix.type           = FB_TYPE_PACKED_PIXELS;
    info->fix.visual         = FB_VISUAL_TRUECOLOR;
    info->fix.xpanstep       = 0;
    info->fix.ypanstep       = 0;
    info->fix.ywrapstep      = 0;
    info->fix.line_length    = dev->width * dev->byte_per_pixel; /* new */
    info->fix.accel          = FB_ACCEL_NONE;
    info->fix.smem_start     = (uint32_t) dev->vmem;
    info->fix.smem_len       = dev->vmemsize;
    info->fix.smem_len       = PAGE_ALIGN(dev->vmemsize);
    info->var.xres           = dev->width;
    info->var.yres           = dev->height;
    info->var.xres_virtual   = dev->width;
    info->var.yres_virtual   = dev->height;
    info->var.bits_per_pixel = dev->byte_per_pixel * 8;
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
    info->pseudo_palette     = NULL;
    info->par                = dev;
    info->flags              = FBINFO_FLAG_DEFAULT
                               | FBINFO_READS_FAST
                               | FBINFO_VIRTFB
                               | FBINFO_HWACCEL_IMAGEBLIT
                               | FBINFO_HWACCEL_FILLRECT
                               | FBINFO_HWACCEL_COPYAREA
                               /*| FBINFO_FOREIGN_ENDIAN*/
                               /*| FBINFO_HWACCEL_YPAN */
    ;

    if (dev->is_fix_endian_used == True)
    {
        printk("usbd480fb: host is big-endian, fixendian16 will be used\n");
    }
    else
    {
        printk("usbd480fb: host is little-endian\n");
    }
    printk("usbd480fb: lcd, %d x %d\n", dev->height, dev->width);
    printk("usbd480fb: lcd, %d byte per pixel\n", dev->byte_per_pixel);
    printk("usbd480fb: lcd, vmode non-interlaced\n");
    printk("usbd480fb: lcd, flags 0x%x\n", info->flags);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating palette ... ");
    info->pseudo_palette = kzalloc(sizeof(uint32_t) * 16, GFP_KERNEL);
    if (info->pseudo_palette == NULL)
    {
        retval = -ENOMEM;
        //dev_err(&interface->dev, "usb480dfb: allocating palette has failed\n");
        printk("failed\n");
        goto error_fbpseudopal;
    }
    else
    {
        memset(info->pseudo_palette, 0x20, sizeof(uint32_t) * 16);
        printk("done\n");
    }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: allocating cmap ... ");
    retval = 1; /* skiped */
//    retval = fb_alloc_cmap(&info->cmap, 256, 0);
    if (retval < 0)
    {
        retval = -ENOMEM;
        //dev_err(&interface->dev, "usb480dfb: allocating cmap has failed\n");
        printk("failed\n");
        goto error_fballoccmap;
    }
    else
    {
        printk("done\n");
    }
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    printk("usb480dfb: registering framebuffer ... ");
    retval = register_framebuffer(info);
    if (retval < 0)
    {
        printk("failed\n");
        goto error_fbreg;
    }
    else
    {
        dev->fbinfo    = info;
        dev->disp_page = 0;
        printk("done\n");
    }

    printk("usbd480fb: mapped as framebuffer device /dev/fb%d, using %ldK of memory\n",
           info->node, (dev->vmemsize >> 10));

    return 0;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 error_wq:
    unregister_framebuffer(info);
 error_fbreg:
// skipped    fb_dealloc_cmap(&info->cmap);
 error_fballoccmap:
    if (info->pseudo_palette)
    {
        kfree(info->pseudo_palette);
    }
 error_fbpseudopal:
    framebuffer_release(info);
 error_fballoc:
    vfree(dev->vmem);
 error_cmd_buf:
 error_usb_buf:
 error_vmem:
    kfree(dev->usb_buf);
    kfree(dev->cmd_buf);
 error_dev_attr:
    goto error_dev;
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
    printk("usbd480fb: probe failed\n");
    return retval;
}



static void usbd480fb_disconnect(struct usb_interface *interface)
{
    struct usbd480fb *dev;
    struct fb_info   *info;

    printk("usbd480: disconnecting ... ");

    dev = usb_get_intfdata(interface);

    goto bypass;
    device_remove_file(&interface->dev, &dev_attr_brightness);
    device_remove_file(&interface->dev, &dev_attr_width);
    device_remove_file(&interface->dev, &dev_attr_height);
    device_remove_file(&interface->dev, &dev_attr_name);
 bypass:

    if (dev != NULL)
    {
        printk("1 ");
        info = dev->fbinfo;
        if (info != NULL)
        {
            printk("2 ");
            unregister_framebuffer(info);
            //skipped
            printk("3 ");
            fb_dealloc_cmap(&info->cmap);
            printk("4 ");
            if (info->pseudo_palette != NULL)
            {
                printk("5 ");
                kfree(info->pseudo_palette);
            }
            else
            {
                printk("usbd480: pseudo_palette is NULL\n");
            }
            framebuffer_release(info);
        }
        else
        {
            printk("usbd480: info is NULL\n");
        }

        if (dev->vmem != NULL)
        {
            printk("6 ");
            vfree(dev->vmem);
        }
        else
        {
            printk("usbd480: vmem is NULL\n");
        }

        if (dev->scr != NULL)
        {
            printk("7 ");
            kfree(dev->scr);
        }
        else
        {
            printk("usbd480: scr is NULL\n");
        }

        if (dev->usb_buf != NULL)
        {
            printk("8 ");
            kfree(dev->usb_buf);
        }
        else
        {
            printk("usbd480: usb_buf is NULL\n");
        }

        if (dev->cmd_buf != NULL)
        {
            printk("9 ");
            kfree(dev->cmd_buf);
        }
        else
        {
            printk("usbd480: cmd_buf is NULL\n");
        }

        printk("10 ");
        //usb_set_intfdata(interface, NULL);
        printk("11 ");
        usb_put_dev(dev->udev);
        printk("12 ");
        kfree(dev);
        printk("13 ");
        //dev_info(&interface->dev, "disconnected\n");
    }
    else
    {
        printk("usbd480: dev is NULL\n");
    }

    if (interface != NULL)
    {
        printk("14 ");
        usb_set_intfdata(interface, NULL);
        printk("15 ");
        dev_info(&interface->dev, "disconnected\n");
    }
    else
    {
        printk("usbd480: interface is NULL\n");
    }
    printk("done\n");

    printk("usbd480fb: disconnected\n");
}

static struct usb_driver usbd480fb_driver =
{
    .name       = "usbd480fb",
    .probe      = usbd480fb_probe,
    .disconnect = usbd480fb_disconnect,
    .id_table   = id_table,
};

static sint16_t __init usbd480fb_init(void)
{
    sint16_t retval = 0;

    retval = usb_register(&usbd480fb_driver);
    if (retval)
        err("usb_register failed. Error number %d", retval);
    return retval;
}

static void __exit usbd480fb_exit(void)
{
    usb_deregister(&usbd480fb_driver);
}

module_init(usbd480fb_init);
module_exit(usbd480fb_exit);

MODULE_AUTHOR("Henri Skippari");
MODULE_DESCRIPTION("USBD480 framebuffer driver");
MODULE_LICENSE("GPL");

