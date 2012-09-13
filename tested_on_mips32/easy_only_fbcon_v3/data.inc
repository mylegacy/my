struct pcf8833
{
    struct fb_info    *info;
    struct spi_device *spi;
    uint16_t          *scr;
};

struct usbd480fb
{
    struct fb_info          *fbinfo; /* ok */
    struct usb_device       *udev;   /* ok */
    uint16_t                *scr;    /* new */

    struct delayed_work     work;    /* to be removed*/
    struct workqueue_struct *wq;     /* to be removed */
    struct kref             kref;    /* to be removed */

/*-----------------------------------------------------*/
    uint8_t                 *vmem;
    uint32_t                vmemsize;
    uint32_t                vmem_phys;
    uint16_t                disp_page;
    uint8_t                 brightness;
    uint16_t                width;
    uint16_t                height;
    uint16_t                byte_per_pixel;     /* new */
    uint16_t                area_size;          /* new, it means how big is vmemsize */
    boolean_t               is_fix_endian_used; /* new, if True, vmemsize is double the phy vram */
    char_t                  device_name[20];
    uint8_t                 *cmd_buf;
    uint8_t                 *usb_buf;
    sint16_t                client_count;
};


