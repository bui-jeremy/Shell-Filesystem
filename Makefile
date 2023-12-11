obj-m += ramdisk_module.o

KERNEL_VERSION := 2.6.33.2

build_module: ramdisk_module.c
	make -C /lib/modules/$(KERNEL_VERSION)/build SUBDIRS=$(PWD) modules

load: build_module
	insmod ramdisk_module.ko

unload:
	rmmod ramdisk_module.ko