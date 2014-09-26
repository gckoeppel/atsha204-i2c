obj-m := atsha204-i2c.o
KDIR ?= /lib/modules/`uname -r`/build
MDIR ?= /lib/modules/`uname -r`/kernel/drivers/char/

all:
	make -C $(KDIR) M=$$PWD modules
clean:
	make -C $(KDIR) M=$$PWD clean
modules_install:
	cp atsha204-i2c.ko $(MDIR)
