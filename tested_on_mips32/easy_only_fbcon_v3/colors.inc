boolean_t is_color_inverted = False;

static void setup_colors(struct fb_info    *info)
{
    /*
       We have shown that the monitor and video card can handle this mode or
       have actually set the mode.  Next the fb_bitfield structure in
       fb_var_screeninfo is filled in. Even if you don't set the mode you get
       a feel of the mode before you really set it. These are typical values
       but may be different for your card. For truecolor modes all the fields
       matter. For pseudocolor modes only the length matters.  Thus all the
       lengths should be the same (=bpp).
     */
    switch (info->var.bits_per_pixel)
    {
    case 1:
    case 8:
        /* Pseudocolor */
        info->var.red.offset    = 0;
        info->var.red.length    = 8;
        info->var.green.offset  = 0;
        info->var.green.length  = 8;
        info->var.blue.offset   = 0;
        info->var.blue.length   = 8;
        info->var.transp.offset = 0;
        info->var.transp.length = 0;
        printk("usbd480fb: using pseudo color mode, %d bits per pixel\n", info->var.bits_per_pixel);
        break;
    case 16:            /* RGB 565 */
        info->var.red.length    = 5;
        info->var.green.length  = 6;
        info->var.blue.length   = 5;
        info->var.transp.offset = 0;
        info->var.transp.length = 0;

        if (is_color_inverted == True)
        {
            /* described set, color inverted */
            info->var.red.offset   = 0;
            info->var.green.offset = 5;
            info->var.blue.offset  = 11;
            printk("usbd480fb: using RGB565 color mode, %d bit per pixel, color inverted\n", info->var.bits_per_pixel);
        }
        else
        {
            /* described set, color not inverted */
            info->var.red.offset   = 11;
            info->var.green.offset = 5;
            info->var.blue.offset  = 0;
            printk("usbd480fb: using RGB565 color mode, %d bit per pixel\n", info->var.bits_per_pixel);
        }
        break;
    case 24:            /* RGB 888 */
        info->var.red.offset    = 0;
        info->var.red.length    = 8;
        info->var.green.offset  = 8;
        info->var.green.length  = 8;
        info->var.blue.offset   = 16;
        info->var.blue.length   = 8;
        info->var.transp.offset = 0;
        info->var.transp.length = 0;
        printk("usbd480fb: using RGB888 color mode, %d bit per pixel\n", info->var.bits_per_pixel);
        break;
    case 32:            /* RGBA 8888 */
        info->var.red.offset    = 0;
        info->var.red.length    = 8;
        info->var.green.offset  = 8;
        info->var.green.length  = 8;
        info->var.blue.offset   = 16;
        info->var.blue.length   = 8;
        info->var.transp.offset = 24;
        info->var.transp.length = 8;
        printk("usbd480fb: using RGBA 8888 color mode, %d bit per pixel\n", info->var.bits_per_pixel);
        break;
    default:
        printk("usbd480fb: using unknown color mode, %d bit per pixel\n", info->var.bits_per_pixel);
        break;
    }
}

static void setup_misc(struct fb_info    *info, struct usbd480fb  *dev)
{
    info->var.bits_per_pixel = BITS_PER_PIXEL;

    info->var.xres         = SCREEN_WIDTH;
    info->var.yres         = SCREEN_HEIGHT;
    info->var.xres_virtual = SCREEN_WIDTH;
    info->var.yres_virtual = SCREEN_HEIGHT;

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    info->var.left_margin  = 0;
    info->var.right_margin = 0;
    info->var.upper_margin = 0;
    info->var.lower_margin = 0;
/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    info->var.vmode = FB_VMODE_NONINTERLACED;

    info->par   = NULL;                /* ??? */
    info->flags = FBINFO_FLAG_DEFAULT; /* no FBINFO_HWACCEL_YPAN */
}
