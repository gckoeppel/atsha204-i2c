ifneq ($(KERNELRELEASE),)
# If KERNELRELEASE is defined then we have been included by the kernel's
# makefiles and are actually building the module.

obj-m := atsha204-i2c.o

# Enable CFLAGS to run DEBUG MODE
#CFLAGS_atsha204-i2c.o := -DDEBUG

else

# To cross-compile this module invoke make with KDIR set to the kernel directory
# and KOPTIONS set to the required extra kernel build options, for example:
#   make KDIR=$MYPROJ/mykernel KOPTIONS="ARCH=arm CROSS_COMPILE=$MYPROJ/toolchain/bin/arm-cortexa5-linux-gnueabihf-"

KDIR ?= /lib/modules/`uname -r`/build
MDIR ?= /lib/modules/`uname -r`/kernel/drivers/char/

SRC := atsha204-i2c.c atsha204-i2c.h
MODULE := atsha204-i2c.ko

.PHONY: all module clean install check modules_install

all: module test/test

module:
	$(MAKE) -C $(KDIR) $(KOPTIONS) M=$(CURDIR) modules

test/test: test/test.c
	$(CC) $^ -o $@

clean:
	$(MAKE) -C $(KDIR) $(KOPTIONS) M=$(CURDIR) clean
	rm -f test/test TAGS

install:
	sudo cp $(MODULE) $(MDIR)
	-sudo insmod $(MODULE)
	-echo atsha204-i2c 0x60 | sudo tee /sys/class/i2c-adapter/i2c-1/new_device
	sudo chgrp i2c /dev/atsha0
	sudo chmod 664 /dev/atsha0

check:
	./test/test

modules_install:
	cp $(MODULE) $(MDIR)

TAGS:
	etags $(SRC)

endif
