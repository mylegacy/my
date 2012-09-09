
/*
 * Copyright (C) 2008 Zsolt Hajdu
 *
 * ssplcd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ssplcd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _N6100FB_H_
#define _N6100FB_H_

#if defined(__KERNEL__)
#   include <linux/types.h>
#else
#   include <inttypes.h>
#endif

#include <linux/ioctl.h>

/*
 * Ioctl definitions
 */
#define N6100FB_IOC_MAGIC    0xcc

// The driver's ioctl command values
#define N6100FB_IOCRESET     _IO(N6100FB_IOC_MAGIC, 0)
#define N6100FB_IOCPASET     _IOW(N6100FB_IOC_MAGIC, 1, int)
#define N6100FB_IOCCASET     _IOW(N6100FB_IOC_MAGIC, 2, int)
#define N6100FB_IOCRAMWR     _IOW(N6100FB_IOC_MAGIC, 3, int)
#define N6100FB_IOCNOP       _IO(N6100FB_IOC_MAGIC, 4)

// 12-bit color definitions
#define WHITE                0xFFF
#define BLACK                0x000
#define RED                  0xF00
#define GREEN                0x0F0
#define BLUE                 0x00F
#define CYAN                 0x0FF
#define MAGENTA              0xF0F
#define YELLOW               0xFF0
#define BROWN                0xB22
#define ORANGE               0xFA0
#define PINK                 0xF6A

/*
 * Used to pass data to the ioctls. Lower byte is
 * sent out first, then the upper (if required).
 */
#define CMDDATA(a, b)    ((b << 8) | a)

#endif // _N6100FB_H_
