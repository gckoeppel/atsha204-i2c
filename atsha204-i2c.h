/* -*- mode: c; c-file-style: "linux" -*- */
/*
 * I2C Driver for Atmel ATSHA204 over I2C
 *
 * Copyright (C) 2014 Mihai Cristea, REDANS SRL, mihai _AT_ redans -DOT- eu
 * Copyright (C) 2014 Josh Datko, Cryptotronix, jbd@cryptotronix.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/device.h>

#define ATSHA204_SLEEP 0x01

struct atsha204_chip {
    struct device *dev;

    int dev_num;
    char devname[7];
    unsigned long is_open;

    struct i2c_client *client;
    struct miscdevice miscdev;
};

static const struct i2c_device_id atsha204_i2c_id[] = {
    {"atsha204-i2c", 0},
    { }
};


/* I2C detection */
static int atsha204_i2c_probe(struct i2c_client *client,
                              const struct i2c_device_id *id);
static int atsha204_i2c_remove(struct i2c_client *client);

/* Device registration */
struct atsha204_chip *atsha204_i2c_register_hardware(struct device *dev);
int atsha204_i2c_add_device(struct atsha204_chip *chip);
void atsha204_i2c_del_device(struct atsha204_chip *chip);
static int atsha204_i2c_release(struct inode *inode, struct file *filep);
static int atsha204_i2c_open(struct inode *inode, struct file *filep);
