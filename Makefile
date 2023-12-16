




KDIR = /lib/modules/`uname -r`/build
CC_TEST       = gcc
CFLAGS_TEST   = -I./ -pedantic -Wall -std=gnu99
LDFLAGS_TEST  =  


obj = test_file.ot ramdisk.ot
test_program = ramdisk_test


obj-m += ramdisk_module.o
ramdisk_module-y = ramdisk_module_main.o


all: kernel_module $(test_program)

kernel_module:
	make -C $(KDIR) M=`pwd` modules

$(test_program): $(obj)
	$(CC_TEST) $(obj) -o $(test_program) $(LDFLAGS_TEST) 

clean:
	make -C $(KDIR) M=`pwd` clean
	rm $(obj) $(test_program) -f
	
%.ot:%.c
	$(CC_TEST) -c $< -o $@ $(CFLAGS_TEST) 


