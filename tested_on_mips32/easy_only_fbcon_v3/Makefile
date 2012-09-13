obj-m = easy3.o
KVERSION = $(shell uname -r)
all: fbcon_map
	@make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	@make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

fbcon_map:

clean_all:
	rm -f *.o
	rm -f *.ko

