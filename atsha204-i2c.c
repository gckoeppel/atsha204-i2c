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
#include <linux/delay.h>

void
atsha204_print_hex_string(const char *str, const u8 *hex, const int len)
{

        int i;

        printk("%s : ", str);

        for (i = 0; i < len; i++)
        {
                if (i > 0) printk(" ");
                printk("0x%02X", hex[i]);
        }

        printk("\n");

}

void atsha204_i2c_set_cmd_parameters(struct atsha204_cmd_metadata *cmd,
                                     u8 opcode)
{
        switch (opcode) {
        case 0x1b:
                printk("%s/n", "RANDOM Command");
                atsha204_set_params(cmd, 32, 50);
                break;
        default:
                atsha204_set_params(cmd, 0, 0);

        }
}

int atsha204_i2c_get_random(const struct i2c_client *client,
                            u8 *to_fill, int bytes)
{
        int rc;
        u8 *rand;

        to_fill = kmalloc(bytes, GFP_KERNEL);
        if (!to_fill)
                return -ENOMEM;

        const u8 rand_cmd[] = {0x03, 0x07, 0x1b, 0x01, 0x00, 0x00, 0x27, 0x47};

        return -1;


}

int atsha204_i2c_transaction(const struct i2c_client *client,
                             const u8* to_send, size_t to_send_len,
                             struct atsha204_buffer *buf)

{
        int rc;
        u8 status_packet[4];
        u8 *recv_buf;
        int total_sleep = 60;
        int packet_len;

        atsha204_print_hex_string("About to send", to_send, to_send_len);

        /* Begin i2c transactions */
        if ((rc = atsha204_i2c_wakeup(client)))
                goto out;

        if ((rc = i2c_master_send(client, to_send, to_send_len)) != to_send_len)
                goto out;

        /* Poll for the response */
        while (4 != i2c_master_recv(client, status_packet, 4) && total_sleep > 0){
                total_sleep = total_sleep - 4;
                msleep(4);
        }

        packet_len = status_packet[0];
        /* The device is awake and we don't want to hit the watchdog
           timer, so don't allow sleeps here*/
        recv_buf = kmalloc(packet_len, GFP_ATOMIC);
        memcpy(recv_buf, status_packet, sizeof(status_packet));
        rc = i2c_master_recv(client, recv_buf + 4, packet_len - 4);

        atsha204_i2c_idle(client);

        buf->ptr = recv_buf;
        buf->len = packet_len;

        atsha204_print_hex_string("Received", recv_buf, packet_len);

        rc = to_send_len;
out:
        return rc;

}

int atsha204_i2c_transmit(const struct i2c_client *client,
                          const char __user *buf, size_t len)
{
        int rc;
        char *to_send;
        u16 crc;
        int len_crc = len + 2;
        u16 *crc_in_buf;

        /* Add room for the CRC16 */
        to_send = kmalloc(len_crc, GFP_KERNEL);
        if (!to_send){
                rc = -ENOMEM;
                goto out;
        }

        printk("%s\n", "Created new buffer");

        if (copy_from_user(to_send, buf, len)){
                rc = -EFAULT;
                goto free_out;
        }

        printk("%s\n", "Copied data new buffer");

        /* The opcode byte is not included in the crc */
        crc = atsha204_crc16(&to_send[1], len - 1);

        printk("%s: %d\n", "CRC", crc);

        crc_in_buf = (u16*)&to_send[len];
        *crc_in_buf = crc;

        atsha204_print_hex_string("About to send", to_send, len_crc);

        /* Begin i2c transactions */
        if ((rc = atsha204_i2c_wakeup(client)))
                goto free_out;

        if ((rc = i2c_master_send(client, to_send, len_crc)) != len_crc)
                goto free_out;

        //i2c_master_recv(chip->client, recv_buf, sizeof(recv_buf));

free_out:
        kfree(to_send);

out:
        return rc;

}

int atsha204_i2c_receive(const struct i2c_client *client,
                         u8 *kbuf, const int len)
{
        u16 *crc_in_buf;
        u16 crc;
        int rc;

        if ((rc = i2c_master_recv(client, kbuf, len)) == len){
                crc_in_buf = &kbuf[len - 2];
                crc = atsha204_crc16(kbuf, len - 2);
                if (!atsha204_check_rsp_crc16(kbuf, len))
                        rc = -EBADMSG;
        }

        return rc;
}

u16 atsha204_crc16(const u8 *buf, const u8 len)
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

int atsha204_i2c_idle(const struct i2c_client *client)
{
        int rc;

        u8 idle_cmd[1] = {0x02};

        rc = i2c_master_send(client, idle_cmd, 1);

        return rc;

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
        struct atsha204_file_priv *priv = filep->private_data;
        struct atsha204_chip *chip = priv->chip;
        u8 *to_send;
        int rc;

        to_send = kmalloc(count, GFP_KERNEL);
        if (!to_send)
                return -ENOMEM;

        if (copy_from_user(to_send, buf, count)){
                rc = -EFAULT;
                return rc;
        }

        rc = atsha204_i2c_transaction(chip->client, to_send, count,
                                      &priv->buf);

        kfree(to_send);

        return rc;
}

ssize_t atsha204_i2c_read(struct file *filep, char __user *buf, size_t count,
                          loff_t *f_pos)
{
        struct atsha204_file_priv *priv = filep->private_data;
        struct atsha204_buffer *r_buf = &priv->buf;

        int rc;

        int size_data = (count > r_buf->len) ? r_buf->len : count;

        if (copy_to_user(buf, &r_buf->ptr[1], size_data)){
                rc = -EFAULT;
        }
        else{
                rc = size_data;
        }

        return rc;
}


static int atsha204_i2c_open(struct inode *inode, struct file *filep)
{
        struct miscdevice *misc = filep->private_data;
        struct atsha204_chip *chip = container_of(misc, struct atsha204_chip,
                                                  miscdev);

        struct atsha204_file_priv *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
        if (NULL == priv)
                return -ENOMEM;

        priv->chip = chip;

        filep->private_data = priv;

        return 0;

}


static int atsha204_i2c_release(struct inode *inode, struct file *filep)
{

        return 0;
}




struct atsha204_chip *atsha204_i2c_register_hardware(struct device *dev,
                                                     struct i2c_client *client)
{

        struct atsha204_chip *chip;

        printk("%s\n", "In register hardware");

        if ((chip = kzalloc(sizeof(*chip), GFP_KERNEL)) == NULL)
                goto out_null;

        chip->dev_num = 0;
        scnprintf(chip->devname, sizeof(chip->devname), "%s%d",
                  "atsha", chip->dev_num);

        chip->dev = get_device(dev);
        dev_set_drvdata(dev, chip);

        chip->client = client;

        if (atsha204_i2c_add_device(chip))
                goto put_device;

        return chip;

put_device:
        put_device(chip->dev);

        kfree(chip);
out_null:
        return NULL;
}


static int atsha204_i2c_probe(struct i2c_client *client,
                              const struct i2c_device_id *id)
{
        int result = -1;
        struct device *dev = &client->dev;

        if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
                return -ENODEV;

        if((result = atsha204_i2c_wakeup(client)) == 0){
                printk("%s", "Device is awake\n");
                atsha204_i2c_sleep(client);

                if (NULL == atsha204_i2c_register_hardware(dev, client))
                        return -ENODEV;

        }
        else{
                printk("%s", "Device failed to wake\n");
        }

        return result;
}

static int atsha204_i2c_remove(struct i2c_client *client)

{
        struct device *dev = &(client->dev);
        struct atsha204_chip *chip = dev_get_drvdata(dev);

        printk("%s\n", "Remove called");

        if (chip)
                misc_deregister(&chip->miscdev);

        kfree(chip);

        /* The device is in an idle state, where it keeps ephemeral
         * memory. Wakeup the device and sleep it, which will cause it
         * to clear its internal memory */

        atsha204_i2c_wakeup(client);
        atsha204_i2c_sleep(client);

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

        printk("%s\n", "In add device");
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
