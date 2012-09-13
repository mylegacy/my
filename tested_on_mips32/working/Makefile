#obj-m = n6100fb_a.o
#obj-m = usbd480fb_new5d.o
#obj-m = usbd480fb_new5c.o
#obj-m = usbd480fb_deferred3.o
#obj-m = new4.o
obj-m = usbd480fb_deferred4b.o
obj-m = usbd480fb_deferred4a.o
obj-m = usbd480fb_deferred4a_my2.o
obj-m = usbd480fb_deferred4a_my3.o
KVERSION = $(shell uname -r)
all: fbcon_map
	@make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	@make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

fbcon_map:

clean_all:
	rm -f *.o
	rm -f *.ko

