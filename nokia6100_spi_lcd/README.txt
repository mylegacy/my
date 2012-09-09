	==========================================================
	N6100FB v0.01 : Nokia 6100 knockoff LCD framebuffer driver
	==========================================================

Summary
-------
This is a framebuffer driver that can be used to display content - mostly text - on a Nokia 6100 compatible lcd screen.
The driver relies on kernel's underlying SPI master/slave architecture to send spi commands/data to the screen.
mmap(), as a way to access and modify the screen content, is not supported, since the video memory cannot be accessed directly. But the /dev/fb0 device can be opened as a regular file, and data written to the file will show up on the screen.
The standard fb_fillrect/fb_copyarea/fb_imageblit hooks also work, so the fbcon driver can use this module to display a console on the screen.

The driver was built and tested with kernel 2.6.24.3 .
It is based on the skeleton framebuffer driver, with spi calls added. The default kernel implementations of the three main callbacks (imageblit/copyarea/fillrect) are also used.
Since the screen's actual video memory cannot be accessed, the driver keeps a the color pixel values in a memory buffer that it allocates and the part of the buffer that changes is always sent over to the the lcd.

Details
-------
As mentioned above, the driver relies on the kernel spi bus being set up.
So in your board configuration module you have to register a spi master device and also declare a slave device that corresponds to the lcd screen. Specify 'n6100fb' as the modalias, so that the kernel will now that this is the driver that should be used with the lcd spi slave device.
With that set up, when the n6100fb module is loaded, first the spi slave driver is registered and then its _probe function is called.

My lcd screen is connected to a Viper-Lite board, running a PXA 255 cpu. This is how it is hooked up to the XScale SPI pins:

 XScale      LCD
-------------------
 GPIO22  |  RESET
 GPIO23  |  SSPSCLK
 GPIO24  |  SSPCS
 GPIO25  |  SSPTXD

Therefore there are a few lines of code in the driver that is specific to my processor and my wiring, pin 22 being used as the reset line in particular. Those calls can be found at the very beginning of the initScreen() function, they all use GPIO_ constants to set up the gpio pins.
You will have to modify those lines so they correspond to the way your hardware is configured.

Building n6100fb
----------------
The Makefile follows the standard kernel module build scheme. You have to edit the Makefile and set the KERNELDIR variable to the path where your kernel source tree is.

After that a simple 'make ARCH=arm' should build the n6100fb.ko module binary file.
Obviously, whatever environment is used to build the kernel itself, has to be used to build this module. So make sure you set CROSS_COMPILE, CC or whatever environment variable(s) it is you have to set.

Since this is a framebuffer driver, the framebuffer feature has to be enabled in the kernel kernel configuration program. The driver also calls cfb_fillrect, cfb_imageblit and cfb_copyarea functions, so those have to be available either built into the kernel or as modules.


Module Parameters
-----------------
There are four parameters currently defined for the module:
  Color mode (colmode): this is the same value as the third parameter of the DATCTL command. The possible values are 2 and 4, the latter being the default. 2 means that pixels are sent to the screen 2 at a time, in three-byte packages. Mode 4 on the other hand lets the pixels sent one at a time, each 12 bit pixel in two-byte packets.

  LCD type (lcdtype) : 0:Epson (the default) or 1:Phillips Since my lcd screen has the Epson controller, I haven't hada chance to test Philips mode at all.

  Hardware scroll (hwscroll) : on or off, the default is off. hwscroll=1 can be added to the command line to turn it on. When it is on, the screen's scroll functionality is used to scroll the screen content.
Basically this is a hack that works in conjunction with fbcon the module, to scroll the console screen content when the screen is full. It relies on fbcon calling the copyarea() callback to move the characters one row up. The driver detects this and uses the hw scroll feature to 'move' the content instead. The result is much faster scroll speed, then full software redraw.

  Vertical flip (yFlip) : Isn't fully implemented yet, but eventually this option will allow the used to have the driver mirror the content vertically. So that the output can be easily adjusted to the hardware arrangement. 


