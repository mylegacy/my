#include <linux/module.h>
#include <linux/moduleparam.h>
//#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/spi/spi.h>

#include <asm/uaccess.h>

//#include <asm/arch/hardware.h>
//#include <asm/arch/pxa-regs.h>
//#include <asm/arch/irqs.h>
#include <asm/errno.h>

#include "n6100fb.h"
#include "lcdconst.h"

#define SCREEN_BAUD    (0x000 << 8)
#define DSS_9          8
#define FRF_SPI        0
#define SSE_ENABLED    (1 << 7)
#define SPO            (1 << 3)
#define SPH            (1 << 4)


/*
 *  Used to initialize the DATCTL P3 value
 */
int colmode = 4;

/* lcdtype defines what kind of controller we're
 * talking to. Also used as an index into the cmdlist arrays.
 *
 */
#define TYPE_EPSON      0
#define TYPE_PHILIPS    1

int lcdtype = TYPE_EPSON;

/* 
 * Can be used to select scroll method, that determines some
 * basic
 * 1 : hardware scroll
 * 0 : software scroll. Basically uses the fbcon REDRAW method
 *     that repaints the whole screen when scrolling is necessary.
 *     But this method may be a better choice in certain cases
 *     where scrolling is not necessary cause characters are placed
 *     at specific screen positions.
 */
int hwscroll = 1;

/*
 * Mirrors the screen content vertically when drawing.
 * 1 : Mirroring ON
 * 0 : Mirroring OFF
 * (Isn't working now)
 */
int yFlip = 0;

enum lcd_cmds
{
    PASET, CASET, RAMWR, NOP
};

unsigned char cmdEpson[4] =
{
    EPSON_PASET, EPSON_CASET, EPSON_RAMWR, EPSON_NOP
};

unsigned char cmdPhil[4] =
{
    PHILIPS_PASET, PHILIPS_CASET, PHILIPS_RAMWR, PHILIPS_NOP
};

unsigned char* cmdList;

module_param(colmode, int, S_IRUGO);
module_param(lcdtype, int, S_IRUGO);
module_param(hwscroll, int, S_IRUGO);
module_param(yFlip, int, S_IRUGO);

#define GPIO_RESET         22

/* The screen size is set up for the default
 * 8x8 character size of fbcon
 */
#define SCREEN_WIDTH       128
#define SCREEN_HEIGHT      128

#define BYTES_PER_PIXEL    4
#define BITS_PER_PIXEL     (BYTES_PER_PIXEL * 8)
#define ROWLEN             (SCREEN_WIDTH * BYTES_PER_PIXEL)

#define RED_SHIFT          16
#define GREEN_SHIFT        8
#define BLUE_SHIFT         0


/*
 * Here are the default fb_fix_screeninfo and fb_var_screeninfo structures
 */
static struct fb_fix_screeninfo n6100fb_fix __devinitdata =
{
    .id          = "N6100",
    .type        = FB_TYPE_PACKED_PIXELS,
    .visual      = FB_VISUAL_TRUECOLOR,
    .xpanstep    =                              1,
    .ypanstep    =                              1,
    .ywrapstep   =                              1,
    .line_length = SCREEN_WIDTH * BYTES_PER_PIXEL,
    .accel       = FB_ACCEL_NONE,
};

static struct fb_var_screeninfo n6100fb_var __devinitdata =
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


struct n6100fb_drv
{
    struct fb_info   * info;     /* FB driver info record */

    struct spi_device* spi;      /* Char device structure */
    u16              * scr;
    u32              pseudo_palette[32];
};

//???#define to_n6100fb_drv(_info) container_of(_info, struct n6100fb_drv, info)

struct n6100fb_drv* main_dev;   /* Allocated dynamically in init function */

static u8         yLcd;
//**********************************************************************
// DELAY

static void n6100fb_delay(int n)
{
    mdelay(n);
}

//**********************************************************************
// SET/CLEAR

static void set_gpio(int gpio)
{
//    GPSR(gpio) = GPIO_bit(gpio);
}

static void clear_gpio(int gpio)
{
//    GPCR(gpio) = GPIO_bit(gpio);
}


//**********************************************************************
// SEND BIT/DATA/COMMAND

static void WriteSpiCommand(unsigned char b)
{
    u16 value;
    value = b;
    spi_write(main_dev->spi, (u8 *) &value, 2);
}

static void WriteSpiData(unsigned char b)
{
    u16 value;

    value = b | 0x100;
    // Write data
    spi_write(main_dev->spi, (u8 *) &value, 2);
}

//#define SENDBYTES 1

// Sends a 16bit word to spi slave
static void WriteSpiDataW(volatile u16 d)
{
#ifdef SENDBYTES
    WriteSpiData(d >> 8);
    WriteSpiData(d & 0xff);
#else
    u32 data = 0x01000100;
    data |= (((u32) d & 0xff00) >> 8);
    data |= ((d & 255) << 16);
    spi_write(main_dev->spi, (u8 *) &data, 4);
#endif
}



/*  Sets rectangular area for pixel manipulation.
 *
 */
static int setWin(int x0, int y0, int x1, int y1)
{
    int t;

    /* put coordinates into ascending order  */
    if (x0 > x1)
    {
        t = x0; x0 = x1; x1 = t;
    }
    if (y0 > y1)
    {
        t = y0; y0 = y1; y1 = t;
    }

    WriteSpiCommand(cmdList[ CASET ]);
    WriteSpiData(x0);
    WriteSpiData(x1);

    WriteSpiCommand(cmdList[ PASET ]);
    WriteSpiData(y0);
    WriteSpiData(y1);

    return (x1 - x0 + 1) * (y1 - y0 + 1); // area
} // setWin

void updateNokiaScreen(struct fb_info* info, unsigned int xPos,
                       unsigned int yPos, unsigned int w, unsigned int h)
{
    int x, y;
//	struct n6100fb_dev* par = info->par;
    u8  *vmem = info->screen_base;
    u8  *src;
    u16 dst;

    //printk( KERN_WARNING "updateNokiaScreen: (%d,%d) - (%dx%d).\n",
    //		   xPos,yPos, w,h );

    if (xPos > SCREEN_WIDTH || yPos > SCREEN_HEIGHT || (xPos + w) > SCREEN_WIDTH ||
        (yPos + h) > SCREEN_HEIGHT)
    {
        printk(KERN_WARNING "updateNokiaScreen: Invalid coordinate. (%d,%d)\n",
               xPos, yPos);
        return;
    }

    // specify the controller drawing box according to those limits
    setWin(xPos, yPos, xPos + w - 1, yPos + h - 1);

    WriteSpiCommand(cmdList[RAMWR]);
    // Send pixels from bottom up, so it shows up correctly on screen
    //for( y=yPos+h-1 ; y>=(int)yPos ; y-- )
    for (y = yPos; y < yPos + h; y++)
    {
        src = (u8 *) (vmem + y * ROWLEN + xPos * BYTES_PER_PIXEL);
        for (x = 0; x < w; x++, src++)
        {
            dst  = (*src++) >> 4;                       // Blue
            dst |= *src++ & 0xf0;
            dst |= ((u16) (*src++ & 0xf0) << 4);
            WriteSpiDataW(dst);
        }
    }
}

void n6100fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    if (hwscroll && 0xff != yLcd)
    {
        struct fb_fillrect r2;
        r2.dx     = rect->dx;
        r2.dy     = yLcd;
        r2.width  = rect->width;
        r2.height = rect->height;
        r2.color  = rect->color;
        r2.rop    = rect->rop;
        cfb_fillrect(info, &r2);

        // Now simply copy to real video buffer
        updateNokiaScreen(info, r2.dx, r2.dy, r2.width, r2.height);
    }
    else
    {
        cfb_fillrect(info, rect);

        // Now simply copy to real video buffer
        updateNokiaScreen(info, rect->dx, rect->dy, rect->width, rect->height);
    }
}

static void n6100fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
//	printk( KERN_WARNING "imageblit:( Src:(%d,%d) %dx%d )\n",
//				image->dx,image->dy,image->width, image->height );
    if (hwscroll && 0xff != yLcd)
    {
        struct fb_image img;
        memcpy(&img, image, sizeof(img));
        img.dy = yLcd;

        cfb_imageblit(info, &img);
        updateNokiaScreen(info, img.dx, img.dy, img.width, img.height);
    }
    else
    {
        /* First copy it into memory buffer */
        cfb_imageblit(info, image);

        /* Now send it to the actual display */
        updateNokiaScreen(info, image->dx, image->dy, image->width, image->height);
    }
}

static void n6100fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
    static u8 inLastLine = 0;
    static u8 startBlock = 0;
    static u8 scrollDone = 0;

    //printk( KERN_WARNING "copyarea:( Src:(%d,%d) Dst:(%d/%d), %dx%d )\n",
    //			area->sx,area->sy,area->dx,area->dy,area->width, area->height );

    cfb_copyarea(info, area);

    if (hwscroll)
    {
        if (120 == area->sy)
        {
            if (area->dy == 112)
            {
                if (0 == inLastLine || 0 == scrollDone)
                {
                    inLastLine  = 1;
                    startBlock += 2;                     // two blocks, 8 pixel rows
                    if (32 == startBlock)
                        startBlock = 0;

                    WriteSpiCommand(EPSON_SCSTART);
                    WriteSpiData(startBlock);
                    scrollDone = 1;

                    if (0xff == yLcd)
                        yLcd = 0;
                    else
                    {
                        yLcd += 8;
                        if (32 * 4 == yLcd)
                            yLcd = 0;
                    }
                }
            }
        }
        else
        {
            inLastLine = 0;
            scrollDone = 0;
        }
    }
    else
        updateNokiaScreen(info, area->dx, area->dy, area->width, area->height);
}

static ssize_t n6100fb_read(struct fb_info *info,
                            char *buf,
                            size_t count,
                            loff_t * ppos)
{
    unsigned long p = *ppos;
    unsigned int  fb_mem_len;

    fb_mem_len = SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL;

    if (p >= fb_mem_len)
        return 0;
    if (count >= fb_mem_len)
        count = fb_mem_len;
    if (count + p > fb_mem_len)
        count = fb_mem_len - p;

    if (count)
    {
        char *base_addr;

        base_addr = info->screen_base;
        count    -= copy_to_user(buf, base_addr + p, count);

        if (!count)
            return -EFAULT;
        *ppos += count;
    }
    return count;
}

static ssize_t n6100fb_write(struct fb_info *info, const char *buf,
                             size_t count, loff_t * ppos)
{
    //struct n6100fb_drv *par = info->par;
    unsigned long p          = *ppos;
    unsigned int  fb_mem_len = SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL;
    int           err;

    if (p > fb_mem_len)
        return -ENOSPC;
    if (count >= fb_mem_len)
        count = fb_mem_len;
    err = 0;
    if (count + p > fb_mem_len)
    {
        count = fb_mem_len - p;
        err   = -ENOSPC;
    }

    if (count)
    {
        char *base_addr;

        base_addr = info->screen_base;
        count    -= copy_from_user(base_addr + p, buf, count);
        *ppos    += count;
        err       = -EFAULT;
    }

    if (count)
    {
        // Always updates whole lines.
        updateNokiaScreen(info, 0, p / ROWLEN, SCREEN_WIDTH,
                          (count + ROWLEN - 1) / ROWLEN);
        return count;
    }
    return err;
}

static void lcdClear(void)
{
    long i;

    printk(KERN_WARNING "Clearing the screen ...");
    setWin(0, 0, 131, 131);
    // set the display memory to BLACK
    WriteSpiCommand(cmdList[RAMWR]);
    if (colmode == 4)
    {
        for (i = 0; i < (132 * 132); i++)
            WriteSpiDataW(BLACK);
    }
    else
    {
        for (i = 0; i < ((132 * 132) / 2); i++)
        {
            WriteSpiData((BLACK >> 4) & 0xFF);
            WriteSpiData(((BLACK & 0xF) << 4) | ((BLACK >> 8) & 0xF));
            WriteSpiData(BLACK & 0xFF);
        }
    }
    printk(KERN_WARNING " ... done.\n");
}

static inline unsigned int chan_to_field(unsigned int chan,
                                         const struct fb_bitfield *bf)
{
    chan  &= 0xffff;
    chan >>= 16 - bf->length;
    return chan << bf->offset;
}

static int n6100fb_setcolreg(unsigned int regno, unsigned int red,
                             unsigned int green, unsigned int blue,
                             unsigned int transp, struct fb_info *info)
{
    unsigned int val;
    u32          *pal;
    int          ret = 1;

    if (info->var.grayscale)
        red = green = blue = (19595 * red + 38470 * green
                              + 7471 * blue) >> 16;

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
    }
    return ret;
}

// Parameter should be
// 1 : for Epson controllers
// 0 : for Philips controllers
//
// Depending on your hardware configuration you probably want to change
// the parameters of the pxa_gpio_mode(..) calls below. These are
//
static void initScreen(int epson)
{
    int i;

    printk("n6100fb: initScreen\n");

    // Pin functions and directions
    //pxa_gpio_mode(GPIO23_SCLK | GPIO_ALT_FN_2_OUT);
    //pxa_gpio_mode(GPIO24_SFRM | GPIO_ALT_FN_2_OUT);
    //pxa_gpio_mode(GPIO25_STXD | GPIO_ALT_FN_2_OUT);
////   pxa_gpio_mode( gGPIOReset | GPIO_OUT );

    // Setup GPIO levels
    set_gpio(GPIO_RESET);

    printk("screen: display reset\n");

    // Reset the display
    clear_gpio(GPIO_RESET);
    n6100fb_delay(10);
    set_gpio(GPIO_RESET);
    n6100fb_delay(10);

    printk("screen: display reset : control\n");

    if (lcdtype == TYPE_PHILIPS)
    {
        cmdList = cmdPhil;
        WriteSpiCommand(PHILIPS_SWRESET);
        WriteSpiCommand(PHILIPS_SLEEPOUT);
        WriteSpiCommand(PHILIPS_INVON);

        WriteSpiCommand(PHILIPS_COLMOD);
        WriteSpiCommand(0x03);

        WriteSpiCommand(PHILIPS_MADCTL);
        WriteSpiCommand(0xC8);
        WriteSpiCommand(PHILIPS_SETCON);
        WriteSpiCommand(0x30);
        n6100fb_delay(10);

        WriteSpiCommand(PHILIPS_DISPON);
    }
    else
    {
        cmdList = cmdEpson;
        // display control
        WriteSpiCommand(EPSON_DISCTL);
        printk("screen: display reset : WriteSpiCommand(DISCTL)\n");
        WriteSpiData(0x00);
        WriteSpiData(0x20);
        WriteSpiData(0x00);

        WriteSpiCommand(EPSON_COMSCN);
        WriteSpiData(0x01);
        WriteSpiCommand(EPSON_OSCON);

        // sleep out
        printk("screen: sleep out\n");
        WriteSpiCommand(EPSON_SLPOUT);
        n6100fb_delay(10);

        // electronic volume, this is kinda contrast/brightness
        //  this might be different for individual LCDs
        WriteSpiCommand(EPSON_VOLCTR);
        WriteSpiData(32);
        WriteSpiData(2);
        //   WriteSpiCommand(VOLDOWN);
        //   WriteSpiCommand(VOLDOWN);

        WriteSpiCommand(EPSON_TMPGRD);
        WriteSpiData(0);
        WriteSpiData(0);
        WriteSpiData(0);
        WriteSpiData(0);
        WriteSpiData(0);

        // power ctrl
        //everything on, no external reference resistors
        WriteSpiCommand(EPSON_PWRCTR);
        WriteSpiData(0x0f);
        n6100fb_delay(10);

        // display mode
        WriteSpiCommand(EPSON_DISINV);

        // datctl
        WriteSpiCommand(EPSON_DATCTL);
        // For my Epson controller
        // Par1: Normal or Reverse Address
        // 00 : (0,0) is in top left (Across from lcd connector)
        // 01 : Seems to be same as 00.

        // P1: 0x01 = page address inverted, column address normal, address scan in column direction
        // P2: 0x00 = RGB sequence (default value)
        // P3: 0x02 = Grayscale -> 16 (selects 12-bit color, type A)
        //     0x04 = Undocumented 12bit color mode with 16bit pixel storage
        WriteSpiData(0 == yFlip ? 2 : 1);
        WriteSpiData(0);
        WriteSpiData((unsigned char) colmode);          // 2

        // display on
        n6100fb_delay(100);
        printk("screen: display on\n");
        WriteSpiCommand(EPSON_DISON);
        n6100fb_delay(10);

        // this loop adjusts the contrast, change the number of iterations to get
        // desired contrast.  this might be different for individual LCDs
        printk("screen: contrast loop\n");
        for (i = 0; i < 50; i++)
        {
            WriteSpiCommand(EPSON_VOLUP);
        }

        // page start/end ram
        WriteSpiCommand(EPSON_PASET);
        // for some reason starts at 2
        WriteSpiData(0x00);
        WriteSpiData(0x83);

        // column start/end ram
        WriteSpiCommand(EPSON_CASET);
        WriteSpiData(0x00);
        WriteSpiData(0x83);

        if (hwscroll)
        {
            // Area Scroll Set
            WriteSpiCommand(EPSON_ASCSET);
            WriteSpiData(0);
            WriteSpiData(31);
            WriteSpiData(31);
            WriteSpiData(1);              // Top Scroll
        }
    }
}


static struct fb_ops n6100fb_ops =
{
    .owner        = THIS_MODULE,
    .fb_read      = n6100fb_read,
    .fb_write     = n6100fb_write,
    .fb_fillrect  = n6100fb_fillrect,
    .fb_copyarea  = n6100fb_copyarea,
    .fb_imageblit = n6100fb_imageblit,
    .fb_setcolreg = n6100fb_setcolreg,
};


/*
 * Driver initialization
 * Spi slave and framebuffer setup.
 */
static int __init n6100fb_probe(struct spi_device* spi)
{
    struct fb_info* info;
    int           res, rc;
//	dev_t		dev = 0;
    unsigned int  memsize    = SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL;
    unsigned char * videoMem = NULL;
    videoMem = vmalloc(memsize);

    yLcd                 = 0xff;
    spi->master->bus_num = 1;
    spi->chip_select     = 0;
    spi->max_speed_hz    = 3686400 / 2;
    spi->mode            = SPI_MODE_0;
    spi->bits_per_word   = 9;
    res                  = spi_setup(spi);
    if (res < 0)
        return res;

    if (NULL == videoMem)
        return -ENOMEM;
    memset(videoMem, 0x0f, memsize);

    n6100fb_fix.smem_start = (unsigned long) videoMem;
    n6100fb_fix.smem_len   = memsize;

    printk(KERN_WARNING "n6100fb: init called,");

    info = framebuffer_alloc(sizeof(struct n6100fb_drv), &spi->dev);
    if (!info)
    {
        res = -ENOMEM;
        goto err;
    }

    info->screen_base = (char __iomem *) videoMem;
    info->fbops       = &n6100fb_ops;
    info->var         = n6100fb_var;
    info->fix         = n6100fb_fix;
    info->flags       = FBINFO_FLAG_DEFAULT;

    if (hwscroll)
    {
        printk(KERN_WARNING " hw scroll mode\n");
        info->flags |= (FBINFO_HWACCEL_COPYAREA | FBINFO_HWACCEL_FILLRECT);
    }
    else
    {
        printk(KERN_WARNING " sw scroll mode\n");
    }

    main_dev             = info->par;
    main_dev->info       = info;
    main_dev->spi        = spi;
    main_dev->scr        = kzalloc(memsize, GFP_KERNEL);
    info->pseudo_palette = main_dev->pseudo_palette;

    initScreen(1);       // Initialize for Epson controller.
    lcdClear();

    /* Register new frame buffer */
    rc = register_framebuffer(main_dev->info);
    if (rc)
    {
        printk(KERN_INFO "Could not register frame buffer\n");
        goto err1;
    }

    printk(KERN_INFO "fb%d: %s frame buffer device, %dK of video memory\n",
           info->node, info->fix.id, memsize >> 10);
    return 0;

 err1:
    framebuffer_release(info);
 err:
    vfree(videoMem);
    vfree(main_dev->scr);

    return res;
}

static int __devexit n6100fb_cleanup(struct spi_device* spi)
{
    struct fb_info    * info = dev_get_drvdata(&spi->dev);
    struct n6100fb_drv* par;

    if (info)
    {
        printk("n6100fb: cleanup\n");

        WriteSpiCommand(EPSON_SLPIN);
        n6100fb_delay(10);
        WriteSpiCommand(EPSON_DISOFF);
        n6100fb_delay(10);

        par = (struct n6100fb_drv *) info->par;
        unregister_framebuffer(info);
        vfree((void __force *) info->screen_base);
        vfree((void __force *) par->scr);
        framebuffer_release(info);
    }
    return 0;
}


/*  The spi client info to connect to the spi bus.
 */

static struct spi_driver n6100fb_driver = 
{
    .driver    = 
    {
        .name  = "n6100fb",
        .bus   = &spi_bus_type,
        .owner = THIS_MODULE,
    },
    .probe  = n6100fb_probe,
    .remove = __devexit_p(n6100fb_cleanup),
};


static int __init n6100fb_init(void)
{
    printk("n6100 compatible spi fb driver\n");
    return spi_register_driver(&n6100fb_driver);
}

static void __exit n6100fb_exit(void)
{
    spi_unregister_driver(&n6100fb_driver);
}


module_init(n6100fb_init);
module_exit(n6100fb_exit);

MODULE_AUTHOR("kernel");
MODULE_DESCRIPTION("Nokia 6100 compatible Framebuffer Driver v0.01");
MODULE_LICENSE("GPL");


