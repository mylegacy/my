#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/vt.h>
#include <linux/kd.h>

extern int errno;

int main (int argc, char* argv[])
{
    struct vt_stat vt_info;
    int fd;
    int ioctl_stat = 0;
    int new_tty;

    if(argc < 2)
        {
        printf("error argv\n");
        exit(1);
        }

    if((fd = open(argv[1] ,O_WRONLY,0)) < 0)
        {
        printf("error opening %s - %s\n",argv[1], strerror(errno));
        exit(5);
        }

    if(ioctl(fd, VT_GETSTATE, &vt_info) == -1)
        {
        printf("error with VT_GETSTATE %s\n",strerror(errno));
        }

    if(ioctl(fd, VT_OPENQRY, &new_tty) == -1)
        {
        printf("error with VT_OPENQRY %s\n",strerror(errno));
        }

    return 0;
}


