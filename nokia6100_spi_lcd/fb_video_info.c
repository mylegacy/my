#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <unistd.h>
#include <termios.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <ctype.h>

#define PUT_SET	0
#define PUT_XOR	1
#define PUT_OR	2
#define PUT_AND 3
#define PUT_NOT	4

#define FONT10	10
#define FONT12	12
#define FONT16	16

#define MAX_X	800
#define MAX_Y	600
#define BYTE_PER_PIXEL	2

typedef union arr4
{
	unsigned char c[4];
	unsigned int  i[2];
	unsigned long l;
}ARRAY4;

static unsigned char* fb_mem;
static FILE *hzk16fp, *hzk12fp, *ascfp;

unsigned int COLOR=65535;
unsigned char PUT_MODE=PUT_SET;
unsigned int TEXT_FGCOLOR=65535;
unsigned int TEXT_BGCOLOR=4;

#define setmode(mode)		PUT_MODE=mode;
#define setcolor(color)		COLOR=color;
#define settext_fg(color)	TEXT_FGCOLOR=color;
#define settext_bg(color)	TEXT_BGCOLOR=color;
int InitFrameBuffer(int* fb)
{
 struct fb_fix_screeninfo finfo;
 struct fb_var_screeninfo vinfo;
 int tty;
 //Open framebuffer device
 *fb = open ( "/dev/fb0", O_RDWR);
 if ( ! (*fb) ){
 	printf("Can't open framebuffer device.\n");
 	return -1;
 }
 //Get framebuffer device infomation
 if( ioctl ( *fb, FBIOGET_FSCREENINFO, &finfo ) ){
 	printf("Error reading fixed framebuffer information.\n");
 	return -2;
 }
 fprintf(stderr, "Printing finfo:\n");
 fprintf(stderr, "tsmem_start = %p\n", (char *)finfo.smem_start);
 fprintf(stderr, "tsmem_len = %d\n", finfo.smem_len);
 fprintf(stderr, "ttype = %d\n", finfo.type);
 fprintf(stderr, "ttype_aux = %d\n", finfo.type_aux);
 fprintf(stderr, "tvisual = %d\n", finfo.visual);
 //fprintf(stderr, "txpanstep = %d\n", finfo.panstep);
 fprintf(stderr, "typanstep = %d\n", finfo.ypanstep);
 fprintf(stderr, "tywrapstep = %d\n", finfo.ywrapstep);
 fprintf(stderr, "tline_length = %d\n", finfo.line_length);
 fprintf(stderr, "tmmio_start = %p\n", (char *)finfo.mmio_start);
 fprintf(stderr, "tmmio_len = %d\n", finfo.mmio_len);
 fprintf(stderr, "taccel = %d\n", finfo.accel);
 
 if( ioctl ( *fb, FBIOGET_VSCREENINFO, &vinfo ) ){
 	printf("Error reading variable framebuffer information.\n");
 	return -3;
 }
 fprintf(stderr, "Printing vinfo:\n");
 fprintf(stderr, "txres: %d\n", vinfo.xres);
 fprintf(stderr, "tyres: %d\n", vinfo.yres);
 fprintf(stderr, "txres_virtual: %d\n", vinfo.xres_virtual);
 fprintf(stderr, "tyres_virtual: %d\n", vinfo.yres_virtual);
 fprintf(stderr, "txoffset: %d\n", vinfo.xoffset);
 fprintf(stderr, "tyoffset: %d\n", vinfo.yoffset);
 fprintf(stderr, "tbits_per_pixel: %d\n", vinfo.bits_per_pixel);
 fprintf(stderr, "tgrayscale: %d\n", vinfo.grayscale);
 fprintf(stderr, "tnonstd: %d\n", vinfo.nonstd);
 fprintf(stderr, "tactivate: %d\n", vinfo.activate);
 fprintf(stderr, "theight: %d\n", vinfo.height);
 fprintf(stderr, "twidth: %d\n", vinfo.width);
 fprintf(stderr, "taccel_flags: %d\n", vinfo.accel_flags);
 fprintf(stderr, "tpixclock: %d\n", vinfo.pixclock);
 fprintf(stderr, "tleft_margin: %d\n", vinfo.left_margin);
 fprintf(stderr, "tright_margin: %d\n", vinfo.right_margin);
 fprintf(stderr, "tupper_margin: %d\n", vinfo.upper_margin);
 fprintf(stderr, "tlower_margin: %d\n", vinfo.lower_margin);
 fprintf(stderr, "thsync_len: %d\n", vinfo.hsync_len);
 fprintf(stderr, "tvsync_len: %d\n", vinfo.vsync_len);
 fprintf(stderr, "tsync: %d\n", vinfo.sync);
 fprintf(stderr, "tvmode: %d\n", vinfo.vmode);
 fprintf(stderr, "      length offset\n");
 fprintf(stderr, "tred: %d/%d\n", vinfo.red.length, vinfo.red.offset);
 fprintf(stderr, "tgreen: %d/%d\n", vinfo.green.length, vinfo.green.offset);
 fprintf(stderr, "tblue: %d/%d\n", vinfo.blue.length, vinfo.blue.offset);
 fprintf(stderr, "talpha: %d/%d\n", vinfo.transp.length, vinfo.transp.offset);
 
 if( ( vinfo.bits_per_pixel/8 != BYTE_PER_PIXEL ) || \
   ( vinfo.xres != MAX_X ) || ( vinfo.yres != MAX_Y ) ){
	printf( "Display mode  error: bits=%d x=%d y=%d\n", vinfo.width/8, vinfo.xres, vinfo.yres );
	return -4;
 }
 
 tty = open ( "/dev/tty", O_RDONLY );
 if( !tty ){
	printf ( "Can't open /dev/tty device\n" );
	return -5;
 }
 
 if( ioctl( tty, KDSETMODE, KD_GRAPHICS ) ){ //KDSETMODE <linux/kd.h>; KD_GRAPHICS <linux/kd.h>
	printf ( "ioctl KDSETMODE\n" );
	return -6;
 }
 close ( tty );


 if((hzk16fp=fopen("hzk16","rb"))==NULL){
 	printf ( "Can't open hzk16 file\n" );
 	return -7;
 }
 
 if((hzk12fp=fopen("hzk12","rb"))==NULL){
 	printf ( "Can't open hzk12 file\n" );
 	return -8;
 }
 
 if((ascfp=fopen("asc1008","rb"))==NULL){
 	printf ( "Can't open asc1008 file\n" );
 	return -9;
 }
 
 fb_mem = mmap ( NULL, MAX_X*MAX_Y*BYTE_PER_PIXEL, PROT_READ|PROT_WRITE, MAP_SHARED, *fb, 0 );
 memset ( fb_mem, 0, MAX_X*MAX_Y*BYTE_PER_PIXEL );//clear screen
 return 0;

}

void CloseFrameBuffer(int *fb)
{ 
  int tty;
  tty = open ( "/dev/tty", O_RDONLY );
  ioctl ( tty, KDSETMODE, KD_TEXT );
  close ( tty );
 
 // memset ( fb_mem, 0, MAX_X*MAX_Y*BYTE_PER_PIXEL );//clear screen
  fclose ( hzk16fp );
  fclose ( hzk12fp );
  fclose ( ascfp );
  close ( *fb );
}

void putpixel ( unsigned int x, unsigned int y )
{
 unsigned long offset;
 int color;
 ARRAY4 oldcolor;
 if( x >= MAX_X ) {
	x=MAX_X-1;
 }
 if( y >=MAX_Y ) {
	y=MAX_Y-1;
 }
 color=COLOR;
 offset = (y*MAX_X*BYTE_PER_PIXEL)+( x*BYTE_PER_PIXEL );
 if( PUT_MODE != PUT_SET ){
	memcpy ( oldcolor.c, fb_mem+offset, BYTE_PER_PIXEL );
	switch( PUT_MODE ){
		case	PUT_XOR:	color^=oldcolor.l; break;
		case	PUT_OR:		color|=oldcolor.l; break;
		case	PUT_AND:	color&=oldcolor.l; break;
		case	PUT_NOT:	color=~color;	   break;
	}
 }
 memset ( fb_mem+offset, color, BYTE_PER_PIXEL );
}

/*-------------------------------------------------*/
/*						   */
/*This function draw a horizontal/vertical gap line*/
/*						   */
/*-------------------------------------------------*/

void putgapline ( unsigned int x1, unsigned int y1, unsigned int x2, unsigned y2 )
{
 unsigned long offset;
 int  i;
 unsigned char step;
 if ( x1 >= MAX_X ) {
	x1 = MAX_X-1;
 }
 if ( x2 >= MAX_X ) {
	x2 = MAX_X-1;
 }
 if ( y1 >= MAX_Y ) {
	y1 = MAX_Y-1;
 }
 if ( y2 >= MAX_Y ) {
	y2 = MAX_Y-1;
 }
 if ( x1 > x2 ) {
	i = x1; x1 = x2; x2 = i;
 }
 if ( y1 > y2 ) {
	i = y1; y1 = y2; y2 = i;
 }

 offset = ( y1*MAX_X*BYTE_PER_PIXEL ) + ( x1*BYTE_PER_PIXEL );
 if( x1 != x2 ) { //horizontal gap line
	unsigned char step=3;
	unsigned int  bytes;
	for ( i = 0; i <= x2-x1; i+=step ) {
		bytes = step*BYTE_PER_PIXEL;
		if ( 3 == step ){
			bytes = step*BYTE_PER_PIXEL;
			memset ( fb_mem+offset, COLOR, bytes);
			offset+=bytes;
			step=2;
		}else{
			offset+=bytes;
			step=3;
		}
	}
 }else{ //vertical gap line
	unsigned int xbytes;
	xbytes=MAX_X*BYTE_PER_PIXEL;
	for ( i = 0; i <= y2-y1; i++ ) {
		if ( i%5 < 3 ) {
			memset ( fb_mem+offset, COLOR, BYTE_PER_PIXEL );
		}
		offset += xbytes;
	}
 }

}

void putline ( unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2 )
{
   unsigned long offset;
   unsigned int i,xbytes;
   int dx,dy,dm,dn,m,n,k,u,v,l,sum;
   char t;
   dx=x2-x1;
   dy=y2-y1;

   offset = ( y1*MAX_X*BYTE_PER_PIXEL ) + ( x1*BYTE_PER_PIXEL );
   if( 0 == dx){
	if( dy < 0 ){
		i = y1;
		y1 = y2;
		y2 = y1;
	}
	xbytes=MAX_X*BYTE_PER_PIXEL;
	for(i=y1;i<=y2;i++){
		memset ( fb_mem+offset, COLOR, BYTE_PER_PIXEL );
		offset += xbytes;
	 }//--for(l=u..)
	return;
   }//---end if(dx==0)---
   
   if( 0 == dy ){
	if( dx < 0 ){
		i = x2;
		x1 = x2;
		x2 = x1;
	}
	memset ( fb_mem+offset, COLOR, (x2-x1)*BYTE_PER_PIXEL );
	return;
   }//---end if(dy==0)---

   dm=1;dn=1;
   if(dy<0){
   	dy=-dy;  dm=-1;
   }
   if(dx<0){
   	dx=-dx;  dn=-1;
   }
   n=dx; m=dy; k=1; v=x1; u=y1;
   if(dy<dx){
	m=dx; n=dy;
	k=dm; dm=dn; dn=k; k=0;
	u=x1; v=y1;
   }
   memset ( fb_mem+offset, COLOR, BYTE_PER_PIXEL );
   
   l=0; sum=m;
   while(sum!=0){
	sum=sum-1;
	l=l+n;
	u=u+dm;
	if(l>=m){
		v=v+dn;
		l=l-m;
	};
	if(k==1){//---putpixel(v,u)---
		offset = ( u*MAX_X*BYTE_PER_PIXEL ) + ( v*BYTE_PER_PIXEL );
		memset ( fb_mem+offset, COLOR, BYTE_PER_PIXEL );
	}else{	//---putpixel(u,v)---
		offset = ( v*MAX_X*BYTE_PER_PIXEL ) + ( u*BYTE_PER_PIXEL );
		memset ( fb_mem+offset, COLOR, BYTE_PER_PIXEL );
	}//--end else
   }//--end while(sum!=0)
}

void fillrect ( unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, int color )
{
 int i;
 int xlen;
 unsigned long offset;
 if ( x1 >= MAX_X ) {
	x1 = MAX_X-1;
 }
 if ( x2 >= MAX_X ) {
	x2 = MAX_X-1;
 }
 if ( y1 >= MAX_Y ) {
	y1 = MAX_Y-1;
 }
 if ( y2 >= MAX_Y ) {
	y2 = MAX_Y-1;
 }
 if ( x2 - x1 < 0 ) {
	i=x2; x2=x1; x1=i;
 }
 if ( y2 - y1 < 0 ) {
	i=y2; x2=x1; x1=i;
 }

 offset = ( y1*MAX_X*BYTE_PER_PIXEL )+( x1*BYTE_PER_PIXEL );
 xlen = ( x2-x1+1 )*BYTE_PER_PIXEL;
 for ( i=y1; i<=y2; i++) {
	memset ( fb_mem+offset, color, xlen );
	offset += MAX_X*BYTE_PER_PIXEL;
 }
 return;
}

void outtext ( unsigned int x, unsigned int y, unsigned char* str, unsigned char font)
{
 int len, i, j, k, l;
 int code, color, step;
 int qh, wh;
 unsigned long offset1;
 unsigned long offset2;
 unsigned long offset3;
 unsigned long offset4;
 unsigned long fontoff;
 unsigned char buff[32];
 unsigned char bytes;
 unsigned char ersline;
 FILE *hzkfp;
 len = strlen ( str );
 offset4 = offset3 = offset2 = offset1 = ( y*MAX_X*BYTE_PER_PIXEL )+( x*BYTE_PER_PIXEL );
 ersline = ( font-FONT10 ) >> 1;
 step = MAX_X*BYTE_PER_PIXEL;
 
 if( FONT12 == font ) {
 	hzkfp = hzk12fp;
	bytes = 24;
 }else{
 	hzkfp = hzk16fp;
	bytes = 32;
 }
 for( i=0; i<len; ){
 	offset2 = offset3;
 	if ( ( code = str[i] ) < 128 ) {//ASCII code
 		if(str[i]=='\n'){ //new line
 			offset4 += MAX_X*BYTE_PER_PIXEL*font;
 			offset3 = offset4;
 		}else{ //common ASCII code
 			for ( j=0; j<ersline; j++ ) {
 				memset ( fb_mem+offset2, TEXT_BGCOLOR, BYTE_PER_PIXEL<<3 );
 				offset2 += step;
 			}
 			fseek ( ascfp, (unsigned int)str[i]*10, SEEK_SET);
 			fread ( buff, 10, 1, ascfp); //read ASCII font mode to buff
 			for ( j=0; j<FONT10; j++ ) {
 				offset1 = offset2;
 				for ( k=0; k<8; k++ ) {
 					if( ( buff[j] << k ) & 0x80 ) {
 						color = TEXT_FGCOLOR;
 					}else{
 						color = TEXT_BGCOLOR;
 					}
 					memset ( fb_mem+offset1, color, BYTE_PER_PIXEL );
 					offset1 += BYTE_PER_PIXEL;
 				}
 				offset2 += step;
 			}
 			for ( j=0; j<ersline; j++ ) {
 				memset ( fb_mem+offset2, TEXT_BGCOLOR, BYTE_PER_PIXEL<<3 );
 				offset2 += step;
 			}
 			offset3 += BYTE_PER_PIXEL<<3;
 		}
 		i++;
 	}else{ // HZ code
		int index;
 		qh=str[i]-0xa0;
		wh=str[i+1]-0xa0;
		fontoff=(94L*(qh-1)+(wh-1))*(unsigned long)bytes;
		fseek (hzkfp,  fontoff, SEEK_SET);
		fread (buff, bytes, 1, hzkfp); //read hzk font mode to buff
		index=0;
		for ( j=0; j<font; j++ ) {
 			offset1 = offset2;
 			for ( l=0; l<2; l++ ){
 				for ( k=0; k<8; k++ ) {
 					if( ( buff[index] << k ) & 0x80 ) {
 						color = TEXT_FGCOLOR;
 					}else{
 						color = TEXT_BGCOLOR;
 					}
 					memset ( fb_mem+offset1, color, BYTE_PER_PIXEL );
 					offset1 += BYTE_PER_PIXEL;
 				}
				index++;
 			}
 			offset2 += step;
 		}
 		offset3 += BYTE_PER_PIXEL<<4;
		i += 2;
	}//end HZ code
 }//end for(..str(len)..) 
}


int main ( void )
{
 int fb;
 int ret;
 int x, y;
 unsigned char str1[]="awdwd\n";
 unsigned char str2[]="alkb\nASDFO+";
 unsigned int color;
 unsigned long offset;
 
 if( ( ret = InitFrameBuffer(&fb) )<0 ){
	printf ("error %d\n",ret);
	return ret;
 }
 
 setcolor( 4 );
 putgapline ( 0, 300, 799, 300 );
 putgapline ( 400, 0, 400, 599 );
 setcolor( 9 );
 putline (  0, 0, 799,   0 );
 putline ( 40, 0,  40, 599 );
// putline ( 0, 0, 799, 599 );
// putline ( 799, 0, 0, 599 );
 outtext ( 3,  100, str1, FONT12 );
 outtext ( 3,  280, str1, FONT16 );
 outtext ( 6,  150, str2, FONT10 );
 outtext ( 7,  170, str2, FONT12 );
 outtext ( 5,  194, str2, FONT16 );
 sleep (10);
 for ( y=0; y<MAX_Y; y++ ) {
	for ( x=0; x<MAX_X; x++ ) {
		setcolor ( color++ );
		putpixel ( x, y);
		//offset = ( (unsigned long)y*MAX_X*BYTE_PER_PIXEL )+( x*BYTE_PER_PIXEL );
		//memset (fb_mem, , 1024 );
	}
 }
 sleep (2);
 setmode(PUT_NOT);
 color=0;
 for( y=0; y<MAX_Y; y++ ) {
	for( x=0; x<MAX_X; x++ ) {
		setcolor ( color++ );
		putpixel ( x, y );
	}
 }
 fillrect (0, 230, 37, 500, 4 );
 sleep (2);
 CloseFrameBuffer(&fb);
 
 /*
 int qh, wh, i, j, k;
 unsigned char buff[32];
 FILE *fp;
 unsigned long fontoff;

 if((fp=fopen("hzk16", "rb"))==NULL){
	return -1;
 }
 for(i=0; i<strlen(str1); ){
	if(str1[i]>128){
		qh=str1[i]-0xa0;
		wh=str1[i+1]-0xa0;
		fontoff=(94*(qh-1)+(wh-1))*32L;
		
		//printf("%04x,%04x,%lx",qh,wh,fontoff);
		fseek ( fp, (94*(qh-1)+(wh-1))*32L, SEEK_SET );
		fread ( buff, 32, 1, fp );
		for(k=0; k<32; k++){
			printf("%02x,",(unsigned char)buff[j*k]);
		}
		printf("\n");
		i+=2;
	}else{
		i++;
	}
	printf("\n");
 }
 fclose(fp);
 */

 return 0;
}

