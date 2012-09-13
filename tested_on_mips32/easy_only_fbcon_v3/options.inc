#ifndef FBINFO_VIRTFB
#define FBINFO_VIRTFB    0x0004 /* FB is System RAM, not device. */ /* from fb.h for older kernels that don't have this defined */
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define FIXENDIAN              yes
boolean_t is_hwscroll = False;
#define SCREEN_WIDTH           480
#define SCREEN_HEIGHT          272
#define lcd_refresh_delay      10
#define USB_TIMEOUT            5000
#define usbd480fb_BKG_COLOR    0x20 // f1
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 * The screen size is set up for the default
 * 8x8 character size of fbcon
 */
#define BYTES_PER_PIXEL        2
#define BITS_PER_PIXEL         (8 * BYTES_PER_PIXEL)
/*
 * with BYTES_PER_PIXE=2
 * info->var.bits_per_pixel = 16;
 */
#define AREASIZE               (SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL * 2)
#define ROWLEN                 (SCREEN_WIDTH * BYTES_PER_PIXEL)
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#define MYTEST1

#undef usbd480fb_VMALLOC
#undef usbd480fb_GETFREEPAGES
#undef usbd480fb_FULLSCREEN_UPDATE
#undef usbd480fb_DEFIO

#ifdef MYTEST1
       #define usbd480fb_VMALLOC
       #define usbd480fb_FULLSCREEN_UPDATE
       #endif

#ifdef MYTEST3
       #define usbd480fb_GETFREEPAGES
       #define usbd480fb_FULLSCREEN_UPDATE
       #endif


