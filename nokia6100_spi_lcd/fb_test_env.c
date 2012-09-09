#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>

int main(void)
{
    int fbfd,fbfd1,fbfd2;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    printf("\nReturn Values of \n\n");

    /* Open the file for reading and writing */
    fbfd = open("/dev/fb0", O_RDWR);
    printf("\nfbfd = %d",fbfd);    
    
    /* Get fixed screen information */
    fbfd1 = ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo); 
    printf("\n fbfd1 = %d",fbfd1);    

    /* Get variable screen information */
    fbfd2 = ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo); 
    printf("\nfbfd2 = %d\n",fbfd2);    

    printf("\n\n%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel );

    close(fbfd);
    return 0;
}

