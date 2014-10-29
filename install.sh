#!/bin/bash

sudo make modules_install
sudo rmmod atsha204-i2c
sudo insmod atsha204-i2c.ko
#0x60 = ATECC108, 0x64 = ATSHA204
#echo atsha204-i2c 0x60 | sudo tee /sys/class/i2c-adapter/i2c-1/new_device
echo atsha204-i2c 0x64 | sudo tee /sys/class/i2c-adapter/i2c-1/new_device
sudo chgrp i2c /dev/atsha0
sudo chmod 664 /dev/atsha0
test/test
