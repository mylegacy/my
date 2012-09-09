

#if defined(__KERNEL__)
#   include <linux/types.h>
#else
#   include <inttypes.h>
#endif

#include <linux/ioctl.h>

#define SCREEN_MAJOR     245

// Epson S1D15G10 Command Set
#define EPSON_DISON      0xaf
#define EPSON_DISOFF     0xae
#define EPSON_DISNOR     0xa6
#define EPSON_DISINV     0xa7
#define EPSON_COMSCN     0xbb
#define EPSON_DISCTL     0xca
#define EPSON_SLPIN      0x95
#define EPSON_SLPOUT     0x94
#define EPSON_PASET      0x75
#define EPSON_CASET      0x15
#define EPSON_DATCTL     0xbc
#define EPSON_RGBSET8    0xce
#define EPSON_RAMWR      0x5c
#define EPSON_RAMRD      0x5d
#define EPSON_PTLIN      0xa8
#define EPSON_PTLOUT     0xa9
#define EPSON_RMWIN      0xe0
#define EPSON_RMWOUT     0xee
#define EPSON_ASCSET     0xaa
#define EPSON_SCSTART    0xab
#define EPSON_OSCON      0xd1
#define EPSON_OSCOFF     0xd2
#define EPSON_PWRCTR     0x20
#define EPSON_VOLCTR     0x81
#define EPSON_VOLUP      0xd6
#define EPSON_VOLDOWN    0xd7
#define EPSON_TMPGRD     0x82
#define EPSON_EPCTIN     0xcd
#define EPSON_EPCOUT     0xcc
#define EPSON_EPMWR      0xfc
#define EPSON_EPMRD      0xfd
#define EPSON_EPSRRD1    0x7c
#define EPSON_EPSRRD2    0x7d
#define EPSON_NOP        0x25


// Philips PCF8833 LCD controller command codes
#define PHILIPS_NOP         0x00    // nop
#define PHILIPS_SWRESET     0x01    // software reset
#define PHILIPS_BSTROFF     0x02    // booster voltage OFF
#define PHILIPS_BSTRON      0x03    // booster voltage ON
#define PHILIPS_RDDIDIF     0x04    // read display identification
#define PHILIPS_RDDST       0x09    // read display status
#define PHILIPS_SLEEPIN     0x10    // sleep in
#define PHILIPS_SLEEPOUT    0x11    // sleep out
#define PHILIPS_PTLON       0x12    // partial display mode
#define PHILIPS_NORON       0x13    // display normal mode
#define PHILIPS_INVOFF      0x20    // inversion OFF
#define PHILIPS_INVON       0x21    // inversion ON
#define PHILIPS_DALO        0x22    // all pixel OFF
#define PHILIPS_DAL         0x23    // all pixel ON
#define PHILIPS_SETCON      0x25    // write contrast
#define PHILIPS_DISPOFF     0x28    // display OFF
#define PHILIPS_DISPON      0x29    // display ON
#define PHILIPS_CASET       0x2A    // column address set
#define PHILIPS_PASET       0x2B    // page address set
#define PHILIPS_RAMWR       0x2C    // memory write
#define PHILIPS_RGBSET      0x2D    // colour set
#define PHILIPS_PTLAR       0x30    // partial area
#define PHILIPS_VSCRDEF     0x33    // vertical scrolling definition
#define PHILIPS_TEOFF       0x34    // test mode
#define PHILIPS_TEON        0x35    // test mode
#define PHILIPS_MADCTL      0x36    // memory access control
#define PHILIPS_SEP         0x37    // vertical scrolling start address
#define PHILIPS_IDMOFF      0x38    // idle mode OFF
#define PHILIPS_IDMON       0x39    // idle mode ON
#define PHILIPS_COLMOD      0x3A    // interface pixel format
#define PHILIPS_SETVOP      0xB0    // set Vop
#define PHILIPS_BRS         0xB4    // bottom row swap
#define PHILIPS_TRS         0xB6    // top row swap
#define PHILIPS_DISCTR      0xB9    // display control
#define PHILIPS_DOR         0xBA    // data order
#define PHILIPS_TCDFE       0xBD    // enable/disable DF temperature compensation
#define PHILIPS_TCVOPE      0xBF    // enable/disable Vop temp comp
#define PHILIPS_EC          0xC0    // internal or external oscillator
#define PHILIPS_SETMUL      0xC2    // set multiplication factor
#define PHILIPS_TCVOPAB     0xC3    // set TCVOP slopes A and B
#define PHILIPS_TCVOPCD     0xC4    // set TCVOP slopes c and d
#define PHILIPS_TCDF        0xC5    // set divider frequency
#define PHILIPS_DF8COLOR    0xC6    // set divider frequency 8-color mode
#define PHILIPS_SETBS       0xC7    // set bias system
#define PHILIPS_RDTEMP      0xC8    // temperature read back
#define PHILIPS_NLI         0xC9    // n-line inversion
#define PHILIPS_RDID1       0xDA    // read ID1
#define PHILIPS_RDID2       0xDB    // read ID2
#define PHILIPS_RDID3       0xDC    // read ID3

