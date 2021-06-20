obj-m += example.o

KDIR = "/home/hoapv/Projects/linux-raspberrypi-kernel_1.20210303-1/"


all:
	make -C $(KDIR) M=`pwd` modules

clean:
	make -C $(KDIR) M=`pwd` clean
