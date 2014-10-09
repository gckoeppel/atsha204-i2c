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

struct i2c_client *client_for_rand = NULL;


void atsha204_print_hex_string(const char *str, const u8 *hex, const int len)
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

int atsha204_i2c_get_random(u8 *to_fill, const size_t max)
{
        int rc;
        struct atsha204_buffer recv = {0,0};
        int rnd_len;
        const struct i2c_client *client = client_for_rand;

        const u8 rand_cmd[] = {0x03, 0x07, 0x1b, 0x01, 0x00, 0x00, 0x27, 0x47};

        printk("In get random\n");

        rc = atsha204_i2c_transaction(client, rand_cmd, sizeof(rand_cmd),
                                      &recv);
        if (sizeof(rand_cmd) == rc){

                if (!atsha204_check_rsp_crc16(recv.ptr, recv.len))
                        rc = -EBADMSG;
                else{
                        rnd_len = (max > recv.len - 3) ? recv.len - 3 : max;
                        memcpy(to_fill, &recv.ptr[1], rnd_len);
                        rc = rnd_len;
                }

        }

        printk("%s %d:%d\n", "Returning random data", rc, max);

        return rc;


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

        /* Store the entire packet. Other functions must check the CRC
           and strip of the length byte */
        buf->ptr = recv_buf;
        buf->len = packet_len;

        atsha204_print_hex_string("Received", recv_buf, packet_len);

        rc = to_send_len;
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

int atsha204_i2c_validate_rsp(const struct atsha204_buffer *packet,
                              struct atsha204_buffer *rsp)
{
        int rc;

        if (packet->len < 4)
                goto out_bad_msg;
        else if (atsha204_check_rsp_crc16(packet->ptr, packet->len)){
                rsp->ptr = packet->ptr + 1;
                rsp->len = packet->len - 3;
                rc = 0;
                goto out;
        }
        else
                /* CRC failed */

out_bad_msg:
        rc = -EBADMSG;
out:
        return rc;
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

        printk("In write\n");

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


int atsha204_i2c_open(struct inode *inode, struct file *filep)
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


int atsha204_i2c_release(struct inode *inode, struct file *filep)
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
        else{
                int rc = hwrng_register(&atsha204_i2c_rng);
                printk("%s%d\n", "HWRNG result: ", rc);
        }


        return chip;

put_device:
        put_device(chip->dev);

        kfree(chip);
out_null:
        return NULL;
}


int atsha204_i2c_probe(struct i2c_client *client,
                              const struct i2c_device_id *id)
{
        int result = -1;
        struct device *dev = &client->dev;
        struct atsha204_chip *chip;

        if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
                return -ENODEV;

        if((result = atsha204_i2c_wakeup(client)) == 0){
                printk("%s", "Device is awake\n");
                atsha204_i2c_idle(client);

                client_for_rand = client;

                if ((chip = atsha204_i2c_register_hardware(dev, client))
                    == NULL){
                        client_for_rand = NULL;
                        return -ENODEV;
                }

                result = atsha204_sysfs_add_device(chip);
        }

        else{
                printk("%s", "Device failed to wake\n");
        }

        return result;
}

int atsha204_i2c_remove(struct i2c_client *client)

{
        struct device *dev = &(client->dev);
        struct atsha204_chip *chip = dev_get_drvdata(dev);

        printk("%s\n", "Remove called");

        if (chip){
                misc_deregister(&chip->miscdev);
                atsha204_sysfs_del_device(chip);
        }

        hwrng_unregister(&atsha204_i2c_rng);

        kfree(chip);

        client_for_rand = NULL;

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

int atsha204_i2c_read4(const struct i2c_client *client, u8 *read_buf,
                       const u16 addr, const u8 param1)
{
        u8 read_cmd[8] = {0};
        u16 crc;
        struct atsha204_buffer rsp, msg;
        int rc, validate_status;

        read_cmd[0] = 0x03; /* Command byte */
        read_cmd[1] = 0x07; /* length */
        read_cmd[2] = 0x02; /* Read command opcode */
        read_cmd[3] = param1;
        read_cmd[4] = cpu_to_le16(addr) & 0xFF;
        read_cmd[5] = cpu_to_le16(addr) >> 8;

        crc = atsha204_crc16(&read_cmd[1], 5);


        read_cmd[6] = cpu_to_le16(crc) & 0xFF;
        read_cmd[7] = cpu_to_le16(crc) >> 8;

        rc = atsha204_i2c_transaction(client, read_cmd, sizeof(read_cmd), &rsp);

        if (sizeof(read_cmd) == rc){
                if ((validate_status = atsha204_i2c_validate_rsp(&rsp, &msg))
                    == 0){
                        atsha204_print_hex_string("Read 4 bytes", msg.ptr, msg.len);
                        memcpy(read_buf, msg.ptr, msg.len);
                        kfree(rsp.ptr);
                        rc = msg.len;
                }
                else
                        rc = validate_status;

        }

        return rc;



}


static ssize_t configzone_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
        struct atsha204_chip *chip = dev_get_drvdata(dev);
        int rc, i;
        u16 bytes, word_addr;
        bool keep_going = true;
        u8 param1 = 0; /* indicates configzone region */
        char *str = buf;

        u8 configzone[128] = {0};

        for (bytes = 0; bytes < sizeof(configzone) && keep_going; bytes += 4){
                word_addr = bytes / 4;
                if (4 != atsha204_i2c_read4(chip->client,
                                            &configzone[bytes],
                                            word_addr, param1)){
                        keep_going = false;
                }
        }

        for (i = 0; i < bytes; i++) {
                str += sprintf(str, "%02X ", configzone[i]);
                if ((i + 1) % 4 == 0)
                        str += sprintf(str, "\n");
        }

        return str - buf;
}
/*static DEVICE_ATTR_RO(configzone);*/
struct device_attribute dev_attr_configzone = __ATTR_RO(configzone);

static ssize_t serialnum_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
        struct atsha204_chip *chip = dev_get_drvdata(dev);
        int i;
        u16 bytes, word_addr;
        bool keep_going = true;
        u8 param1 = 0; /* indicates configzone region */
        char *str = buf;

        u8 serial[12] = {0};

        for (bytes = 0; bytes < sizeof(serial) && keep_going; bytes += 4){
                word_addr = bytes / 4;
                if (4 != atsha204_i2c_read4(chip->client,
                                            &serial[bytes],
                                            word_addr, param1)){
                        keep_going = false;
                }
        }

        for (i = 0; i < bytes; i++) {
                str += sprintf(str, "%02X", serial[i]);
                if ((i + 1) % sizeof(serial) == 0)
                        str += sprintf(str, "\n");
        }

        return str - buf;
}
/*static DEVICE_ATTR_RO(configzone);*/
struct device_attribute dev_attr_serialnum = __ATTR_RO(serialnum);


static struct attribute *atsha204_dev_attrs[] = {
        &dev_attr_configzone.attr,
        &dev_attr_serialnum.attr,
        NULL,
};

static const struct attribute_group atsha204_dev_group = {
        .attrs = atsha204_dev_attrs,
};

int atsha204_sysfs_add_device(struct atsha204_chip *chip)
{
        int err;
        err = sysfs_create_group(&chip->dev->kobj,
                                 &atsha204_dev_group);

        if (err)
                dev_err(chip->dev,
                        "failed to create sysfs attributes, %d\n", err);
        return err;
}

void atsha204_sysfs_del_device(struct atsha204_chip *chip)
{
        sysfs_remove_group(&chip->dev->kobj, &atsha204_dev_group);
}

MODULE_AUTHOR("Josh Datko <jbd@cryptotronix.com");
MODULE_DESCRIPTION("Atmel ATSHA204 driver");
//MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
