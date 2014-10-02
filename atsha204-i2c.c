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
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/uaccess.h>
#include "atsha204-i2c.h"
#include <linux/crc16.h>
#include <linux/bitrev.h>

static u8 recv_buf[64];


u16 atsha204_crc16(u8 *buf, u8 len)
{
        u8 i;
        u16 crc16 = 0;

        for (i = 0; i < len; i++) {
                u8 shift;

                for (shift = 0x01; shift > 0x00; shift <<= 1) {
                        u8 data_bit = (buf[i] & shift) ? 1 : 0;
                        u8 crc_bit = crc16 >> 15;

                        crc16 <<= 1;

                        if ((data_bit ^ crc_bit) != 0)
                                crc16 ^= 0x8005;
                }
        }

        return cpu_to_le16(crc16);
}

bool atsha204_crc16_matches(const u8 *buf, const u8 len, const u16 crc)
{
        u16 crc_calc = atsha204_crc16(buf,len);
        return (crc == crc_calc) ? true : false;
}

bool atsha204_check_rsp_crc16(const u8 *buf, const u8 len)
{
        u16 *rec_crc = &buf[len - 2];
        return atsha204_crc16_matches(buf, len - 2, cpu_to_le16(*rec_crc));
}

void print_crc(u8 *buf, u8 size)
{
        printk("%s\n", "CRC START");
        u16 crc = crc16(0, buf, size);
        printk("CRC: %d\n", crc);

        crc = crc16(0xFF,buf,size);
        printk("CRC: %d\n", crc);

        crc = atsha204_crc16(buf, size);
        printk("CRC: %d\n", crc);

        printk("%s\n", "CRC END");
}

int atsha204_i2c_wakeup(const struct i2c_client *client)
{
        bool is_awake = false;
        int retval = -ENODEV;

        u8 buf[4] = {0};

        while (!is_awake){
                if (4 == i2c_master_send(client, buf, 4)){
                        printk("%s\n", "OK, we're awake.");
                        is_awake = true;

                        if (4 == i2c_master_recv(client, buf, 4)){
                                printk("%s", "Received wakeup: ");
                                printk("%x:", buf[0]);
                                printk("%x:", buf[1]);
                                printk("%x:", buf[2]);
                                printk("%x\n", buf[3]);
                        }


                        u16 *rec_crc = &buf[2];

                        if (atsha204_check_rsp_crc16(buf,4))
                                retval = 0;
                        else
                                printk("%s\n", "CRC failure");


                }
                else{
                        /* is_awake is already false */
                }
        }

        retval = 0;
        return retval;

}

int atsha204_i2c_sleep(const struct i2c_client *client)
{
        int retval;
        char to_send[1] = {ATSHA204_SLEEP};

        if ((retval = i2c_master_send(client,to_send,1)) == 1)
                retval = 0;

        printk("Device sleep status: %d\n", retval);
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

static int atsha204_i2c_open(struct inode *inode, struct file *filep)
{

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



struct atsha204_chip *atsha204_i2c_register_hardware(struct device *dev)
{
        struct atsha204_chip *chip;

        if ((chip = kzalloc(sizeof(*chip), GFP_KERNEL)) == NULL)
                goto out_null;

        chip->dev_num = 0;
        scnprintf(chip->devname, sizeof(chip->devname), "%s%d",
                  "atsha", chip->dev_num);

        chip->dev = get_device(dev);
        dev_set_drvdata(dev, chip);

        if (atsha204_i2c_add_device(chip))
                goto put_device;

        return chip;

put_device:
        put_device(chip->dev);
out_free:
        kfree(chip);
out_null:
        return NULL;
}

static int atsha204_i2c_release(struct inode *inode, struct file *filep)
{

}
void atsha204_i2c_del_device(struct atsha204_chip *chip)
{
        if (chip->miscdev.name)
                misc_deregister(&chip->miscdev);
}

static int atsha204_i2c_probe(struct i2c_client *client,
                              const struct i2c_device_id *id)
{
        int result = -1;
        struct atsha204_chip *chip;
        struct device *dev = &client->dev;

        if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
                return -ENODEV;

        if(0 == atsha204_i2c_wakeup(client)){
                printk("%s", "Device is awake\n");
                atsha204_i2c_sleep(client);

                result = 0;
        }
        else{
                printk("%s", "Device failed to wake\n");
        }

        return 0;
}

static int atsha204_i2c_remove(struct i2c_client *client)

{
        printk("%s\n", "Remove called");
        return 0;
}





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


int atsha204_i2c_add_device(struct atsha204_chip *chip)
{
        int retval;

        chip->miscdev.fops = &atsha204_i2c_fops;
        chip->miscdev.minor = MISC_DYNAMIC_MINOR;

        chip->miscdev.name = chip->devname;
        chip->miscdev.parent = chip->dev;

        if ((retval = misc_register(&chip->miscdev)) != 0){
                chip->miscdev.name = NULL;
                dev_err(chip->dev,
                        "unable to misc_register %s, minor %d err=%d\n",
                        chip->miscdev.name,
                        chip->miscdev.minor,
                        retval);
        }


        return retval;
}

MODULE_AUTHOR("Josh Datko <jbd@cryptotronix.com");
MODULE_DESCRIPTION("Atmel ATSHA204 driver");
//MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
