*  app-i18n/fbiterm
      Homepage:      http://www-124.ibm.com/linux/projects/iterm/
      Description:   Framebuffer internationalized terminal emulator


he framebuffer console (fbcon), as its name implies, is a text console running on top of the framebuffer device. It has the functionality of any standard text console driver, such as the VGA console, with the added features that can be attributed to the graphical nature of the framebuffer.

What are the features of fbcon? The framebuffer console supports high resolutions, varying font types, display rotation, primitive multihead, etc. Theoretically, multi-colored fonts, blending, aliasing, and any feature made available by the underlying graphics card are also possible.



Configuration

The framebuffer console can be enabled by using your favorite kernel configuration tool. It is under Device Drivers→Graphics Support→Support for framebuffer devices→Framebuffer Console Support. Select 'y' to compile support statically, or 'm' for module support. The module will be fbcon.

You should also enable Virtual terminal and VT Console Support
Device Drivers --->
  Character devices --->
    [*] Virtual terminal
    [*]   Support for console on virtual terminal
    [ ]   Support for binding and unbinding console drivers
    [ ] Non-standard serial port support
        Serial drivers  --->
    [*] Unix98 PTY support
    [ ] Legacy (BSD) PTY support
Virtual terminal

If you say Y here, you will get support for terminal devices with display and keyboard devices. These are called “virtual” because you can run several virtual terminals (also called virtual consoles) on one physical terminal.

You need at least one virtual terminal device in order to make use of your keyboard and monitor. Therefore, only people configuring an embedded system would want to say N here in order to save some memory; the only way to log into such a system is then via a serial or network connection.

Support for console on virtual terminal

The system console is the device which receives all kernel messages and warnings and which allows logins in single user mode. If you answer Y here, a virtual terminal (the device used to interact with a physical terminal) can be used as system console. This is the most common mode of operations, so you should say Y here unless you want the kernel messages be output only to a serial port (in which case you should say Y to “Console on serial port”, below).

If you do say Y here, by default the currently visible virtual terminal (/dev/tty0) will be used as system console. You can change that with a kernel command line option such as “console=tty3” which would use the third virtual terminal as system console. See the documentation of your boot loader (u-boot) about how to pass options to the kernel at boot time.

Modify the kernel command line

When the kernel boot the command line is passed into the kernel from u-boot in the “bootargs” environment variable or the kernel command line can be compiled in to the kernel if required.

To trigger the Framebuffer Console instead of the Serial Console the kernel command line may contain:

console=tty0
To use multiple consoles, you can simply do something like:

console=tty0 console=ttyBF0,57600
The last console specified will be the default.

Framebuffer Driver

In order for fbcon to activate, at least one framebuffer driver is required, so choose from any of the numerous drivers available.

Device Drivers --->
  Graphics support --->
    [*] Backlight & LCD device support  --->
        <*> Lowlevel Backlight controls
        <*> Lowlevel LCD controls
    <*> Support for frame buffer devices
    <*> Support for frame buffer devices
    [*]   Enable firmware EDID
    [ ]   Enable Video Mode Handling Helpers
    [ ]   Enable Tile Blitting Support
    ---   Frame buffer hardware drivers
    <*> SHARP LQ035 TFT LCD on uClinux (BF537 STAMP)
    (0x58) The slave address of the I2C device (0x58, 0x5A, 0x5C and 0x5E)
    [*]   Use Landscape 320x240 instead of Portrait 240x320
    [ ]   Use 16-bit BGR-565 instead of RGB-565
    < > Virtual Frame Buffer support (ONLY FOR TESTING!)
        Console display driver support  --->
        Logo configuration  --->
Blackfin/Linux currently supports a variety of frame buffer output devices:

adv7171 - an Analog Devices Video DAC which accepts data in the YCrCb color space
bf537-lq035 - Sharp 3.5 inch QVGA Display
bfin-lq035q1-fb Sharp 3.5 inch QVGA Display
adv7393 - an Analog Devices Video DAC which accepts data in RGB color space
bf54x-lq043fb - Sharp 4.3 inch 480×272 TFT LCD Display (BF548 EZ-Kit)
Varitronix VL_PS_COG_T350MCQB TFT - The BF527 EZ-KIT Lite features a Varitronix VL_PS_COG_T350MCQB TFT LCD module with touchscreen overlay. This is a 3.5” landscape display with a resolution of 320 x 240 and a color depth of 24 bits. The interface is an RGB-888 serial parallel interface, eight bits of red, followed by eight bits of green, and then eight bits of blue.
Related:
Interface 16/18-bit TFT-LCD over an 8-bit wide PPI using a small Programmable Logic Device (CPLD)
Design Files and Schematics

See also the page about programs that work with the framebuffer.

Fonts

Also, you will need to select at least one compiled-in fonts, but if you don't do anything, the kernel configuration tool will select one for you, usually an 8×16 font.

Device Drivers --->
  Graphics support --->
    Console display driver support  --->
      <*> Framebuffer Console support
      [ ]   Framebuffer Console Rotation
      [*] Select compiled-in fonts
      [ ]   VGA 8x8 font
      [ ]   VGA 8x16 font
      [*]   Mac console 6x11 font (not supported by all drivers)
      [ ]   console 7x14 font (not supported by all drivers)
      [ ]   Pearl (old m68k) console 8x8 font
      [ ]   Acorn console 8x8 font
      [ ]   Mini 4x6 font
      [ ] Sparc console 8x16 font
      [ ] Sparc console 12x22 font (not supported by all drivers)
      [ ] console 10x18 font (not supported by all drivers)
Sample screenshots from an QVGA 320x240 display

