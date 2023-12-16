obj-m += ramdisk_module.o
ramdisk_module-objs := ramdisk_kernel.o module.o

all:	
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	insmod ramdisk_module.ko
clean:
	rmmod ramdisk_module.ko
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
