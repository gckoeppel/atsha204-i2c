obj-m := atsha204-i2c.o
KDIR ?= /lib/modules/`uname -r`/build
MDIR ?= /lib/modules/`uname -r`/kernel/drivers/char/
SRC = atsha204-i2c.c atsha204-i2c.h
# Enable CFLAG to run DEBUG MODE
#CFLAGS_atsha204-i2c.o := -DDEBUG

all:
	make -C $(KDIR) M=$$PWD modules
#	make testing code
	gcc -c $$PWD/test/test.c
	gcc $$PWD/test/test.c -o $$PWD/test/test

clean:
	make -C $(KDIR) M=$$PWD clean
	-rm -rf $$PWD/test/test.o $$PWD/test/test TAGS

install:
	sudo cp atsha204-i2c.ko $(MDIR)
	-sudo insmod atsha204-i2c.ko
	-echo atsha204-i2c 0x60 | sudo tee /sys/class/i2c-adapter/i2c-1/new_device
	sudo chgrp i2c /dev/atsha0
	sudo chmod 664 /dev/atsha0


check:
	./test/test

modules_install:
	cp atsha204-i2c.ko $(MDIR)

TAGS:
	etags $(SRC)
