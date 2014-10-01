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
#include <linux/uaccess.h>

static u8 recv_buf[64];

struct atsha204_chip {
        struct device *dev;

        int dev_num;
        char devname[7];
        unsigned long is_open;

        struct i2c_client *client;
};

int atsha204_i2c_wakeup(const struct i2c_client *client)
{
        bool is_awake = false;
        int retval = -ENODEV;

        u8 buf[4] = {0};

        while (!is_awake){
                if (4 == i2c_master_send(client, buf, 4)){
                        is_awake = true;
                }
                else{
                        /* is_awake is already false */
                }
        }

        retval = 0;
        return retval;

}

ssize_t atsha204_i2c_write(struct file *filep, const char __user *buf,
                           size_t count, loff_t *f_pos)
{
        char *to_send;

        struct atsha204_chip *chip = filep->private_data;

        ssize_t retval = -ENOMEM;

        to_send = kmalloc(count, GFP_KERNEL);
        if (!to_send)
                goto out;

        if (copy_from_user(to_send, buf, count)){
                retval = -EFAULT;
                goto out;
        }

        if ((retval = atsha204_i2c_wakeup(chip->client)) != 0)
                goto free;

        if ((retval = i2c_master_send(chip->client, to_send, count)) != count){
                goto free;
        }

        i2c_master_recv(chip->client, recv_buf, sizeof(recv_buf));

free:
        kfree(to_send);
out:
        return retval;


}

ssize_t atsha204_i2c_read(struct file *filep, char __user *buf, size_t count,
                          loff_t *f_pos)
{
        int retval = 0;

        int size_data = (count > sizeof(recv_buf)) ? sizeof(recv_buf) : count;

        if (copy_to_user(buf, recv_buf, size_data)){
                retval = -EFAULT;
        }
        else{
                retval = size_data;
        }

        return size_data;
}

static int atsha204_i2c_probe(struct i2c_client *client,
                              const struct i2c_device_id *id)
{
        int result = -1;
        if(0 == atsha204_i2c_wakeup(client)){
                printk("%s", "Device is awake\n");
                result = 0;
        }
        else{
                printk("%s", "Device failed to wake\n");
        }

        return result;
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

static int __init atsha204_i2c_init(void)
{
        return i2c_add_driver(&atsha204_i2c_driver);
}


static void __exit atsha204_i2c_driver_cleanup(void)
{
        i2c_del_driver(&atsha204_i2c_driver);
}
module_init(atsha204_i2c_init);
module_exit(atsha204_i2c_driver_cleanup);

static const struct file_operations atsha204_i2c_fops = {
        .owner = THIS_MODULE,
        .llseek = no_llseek,
        .open = atsha204_i2c_open,
        .read = atsha204_i2c_read,
        .write = atsha204_i2c_write,
        .release = atsha204_i2c_release,
};


MODULE_AUTHOR("Josh Datko <jbd@cryptotronix.com");
MODULE_DESCRIPTION("Atmel ATSHA204 driver");
//MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
