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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/i2c.h>

int atsha204_i2c_wakeup(const struct i2c_client *client)
{
        u8 buf[4] = {0};

        return (4 == i2c_master_send(client, buf, 4)) ? 0 : -1;

}
static int atsha204_i2c_probe(struct i2c_client *client,
                              const struct i2c_device_id *id)
{
        if(0 == atsha204_i2c_wakeup(client)){
                printk("%s", "Device is awake\n");
        }
        else{
                printk("%s", "Device failed to wake\n");
        }

        return 0;
}

static int atsha204_i2c_remove(struct i2c_client *client)

{
        printk("%s", "Remove called");
        return 0;
}



static const struct i2c_device_id atsha204_i2c_id[] = {
        {"atsha204-i2c", 0},
        { }
};

MODULE_DEVICE_TABLE(i2c, atsha204_i2c_id);

static struct i2c_driver atsha204_i2c_driver = {
        .driver = {
                .name = "atsha204-i2c",
                .owner = THIS_MODULE,
        },
        .probe = atsha204_i2c_probe,
        .remove = atsha204_i2c_remove,
        .id_table = atsha204_i2c_id,
};

module_i2c_driver(atsha204_i2c_driver);

MODULE_AUTHOR("Josh Datko <jbd@cryptotronix.com");
MODULE_DESCRIPTION("Atmel ATSHA204 driver");
//MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
