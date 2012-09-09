obj-m = n6100fb.o
#obj-m = usbd480fb_new4.o
KVERSION = $(shell uname -r)
all: fbcon_map
	@make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	@make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

fbcon_map:


