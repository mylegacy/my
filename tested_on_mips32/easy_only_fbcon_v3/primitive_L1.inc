static sint16_t usbd480fb_ops_mmap
(
    struct fb_info *info,
    struct vm_area_struct *vma
)
{
    printk("usbd480db: ops_mmap, not been implemented yet\n");
    return 0;
}

static void usbd480fb_ops_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
    struct usbd480fb *dev;

    dev = info->par;
    sys_copyarea(info, area);
    printk("usbd480db: copyarea\n");
    usbd480fb_update_rect(dev, area->dx, area->dy, area->width, area->height);
}

static void usbd480fb_ops_imageblit(struct fb_info *info, const struct fb_image *image)
{
    struct usbd480fb *dev;

    dev = info->par;
    sys_imageblit(info, image);
    printk("usbd480db: imageblit (%d,%d),(%d,%d)\n",
           image->dx, image->dy, image->width, image->height);
    usbd480fb_update_rect(dev, image->dx, image->dy, image->width, image->height);
}

static void usbd480fb_ops_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    struct usbd480fb *dev;

    dev = info->par;
    sys_fillrect(info, rect);
    printk("usbd480db: fillrect (%d,%d,%d,%d)\n",
           rect->dx, rect->dy, rect->width, rect->height);
    usbd480fb_update_rect(dev, rect->dx, rect->dy, rect->width, rect->height);
}

uint32_t flow_total;
static sint16_t usbd480fb_ops_open(struct fb_info *info, sint16_t user)
{
    struct usbd480fb *dev;

    dev        = info->par;
    flow_total = 0;
    dev->client_count++;
    printk("usbd480db: open\n");
    printk("usbd480fb: fb%d user=%d new client, total %d\n", info->node, user, dev->client_count);
    return 0;
}

static sint16_t usbd480fb_ops_release(struct fb_info *info, sint16_t user)
{
    struct usbd480fb *dev;

    dev = info->par;
    dev->client_count--;
    printk("usbd480db: release, flowtotal %ld\n", flow_total);
    printk("usbd480fb: fb%d user=%d release client, total %d\n", info->node, user, dev->client_count);
    return 0;
}

static sint16_t usbd480fb_ops_setcolreg
(
    uint16_t regno,
    uint16_t red,
    uint16_t green,
    uint16_t blue,
    uint16_t transp,
    struct fb_info *info
)
{
    uint32_t c;
    sint16_t ans;

    ans = 0;

    printk("usbd480fb: fb%d setcolreg %d r:%d g:%d b:%d .. ", info->node, regno, red, green, blue);

    if (regno > 255)
    {
        printk("failed\n");
        ans = 1;
    }
    else
    {
        red   >>= 11;
        green >>= 10;
        blue  >>= 11;

        if (regno < 16)
        {
            c = ((red & 0x3f) << info->var.red.offset) |
                ((green & 0x7f) << info->var.green.offset) |
                ((blue & 0x3f) << info->var.blue.offset);

            ((u32 *) info->pseudo_palette)[regno] = c;
            printk("done\n");
            ans = 0;
        }
    }
    return ans;
}

static ssize_t usbd480fb_ops_write
(
    struct fb_info *info,
    const char_t *buf,
    size_t count,
    loff_t *ppos
)
{
    struct usbd480fb *dev;
    ssize_t          ret;
    uint32_t         area;
    uint32_t         pos;
    uint32_t         rest;

    dev  = info->par;
    pos  = *ppos;
    //area = (dev->width * dev->height);
    area = dev->area_size;

    ret = 0;

    if (dev->area_size < pos)
    {
        rest = 0;
    }
    else
    {
        rest = dev->area_size - pos;
        ret  = fb_sys_write(info, buf, count, ppos);

        printk("usbd480db: write, count=%d, pos=%ld rest=%ld\n", count, pos, rest);

        flow_total += count; /* my indicator, total byte written to fb */

        //if ( ((pos + count) / 2) <= (area))
        if ( ((pos + count)) <= (area))
        {
            printk("usbd480db: write, count=%d, pos=%ld rest=%ld updated area = %ld\n", count, pos, rest, area);
            usbd480fb_update_line(info->par, pos / 2, count / 2);
        }
        else if (rest <= count )
        {
            printk("usbd480db: write, count=%d, pos=%ld rest=%ld coda area = %ld\n", count, pos, rest, area);
            usbd480fb_update_line(info->par, pos, rest);
        }

    }

    return ret;
}

