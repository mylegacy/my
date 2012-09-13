/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define FIXENDIAN            yes
boolean_t is_hwscroll = False;
#define SCREEN_WIDTH         480
#define SCREEN_HEIGHT        272
#define lcd_refresh_delay    10
#define USB_TIMEOUT          5000
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 * The screen size is set up for the default
 * 8x8 character size of fbcon
 */
#define BYTES_PER_PIXEL      2
#define BITS_PER_PIXEL       (8 * BYTES_PER_PIXEL)
/*
 * with BYTES_PER_PIXE=2
 * info->var.bits_per_pixel = 16;
 */
#define AREASIZE             (SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL * 2)
#define ROWLEN               (SCREEN_WIDTH * BYTES_PER_PIXEL)
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#define MYTEST3

#undef USBD480_VMALLOC
#undef USBD480_GETFREEPAGES
#undef USBD480_FULLSCREEN_UPDATE
#undef USBD480_DEFIO

#ifdef MYTEST1
       #define USBD480_VMALLOC
       #define USBD480_FULLSCREEN_UPDATE
       #endif

#ifdef MYTEST2
       #define USBD480_VMALLOC
       #define USBD480_DEFIO
       #endif

#ifdef MYTEST3
       #define USBD480_GETFREEPAGES
       #define USBD480_FULLSCREEN_UPDATE
       #endif

#ifdef MYTEST4
       #define USBD480_GETFREEPAGES
       #define USBD480_DEFIO
       #endif


