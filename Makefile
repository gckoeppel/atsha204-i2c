obj-m := atsha204-i2c.o
KDIR ?= /lib/modules/`uname -r`/build
MDIR ?= /lib/modules/`uname -r`/kernel/drivers/char/
SRC = atsha204-i2c.c atsha204-i2c.h

all:
	make -C $(KDIR) M=$$PWD modules
clean:
	make -C $(KDIR) M=$$PWD clean
	rm TAGS
modules_install:
	cp atsha204-i2c.ko $(MDIR)
TAGS:
	etags $(SRC)
