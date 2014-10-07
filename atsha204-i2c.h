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

struct atsha204_cmd_metadata {
    int expected_rec_len;
    int actual_rec_len;
    unsigned long usleep;
};

struct atsha204_buffer {
    u8 *ptr;
    int len;
};

struct atsha204_file_priv {
    struct atsha204_chip *chip;
    struct atsha204_cmd_metadata meta;

    struct atsha204_buffer buf;
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
struct atsha204_chip *atsha204_i2c_register_hardware(struct device *dev,
                                                     struct i2c_client *client);
int atsha204_i2c_add_device(struct atsha204_chip *chip);
void atsha204_i2c_del_device(struct atsha204_chip *chip);
static int atsha204_i2c_release(struct inode *inode, struct file *filep);
static int atsha204_i2c_open(struct inode *inode, struct file *filep);

/* atsha204 crc functions */
u16 atsha204_crc16(const u8 *buf, const u8 len);
bool atsha204_check_rsp_crc16(const u8 *buf, const u8 len);

/* atsha204 specific functions */
int atsha204_i2c_wakeup(const struct i2c_client *client);
int atsha204_i2c_idle(const struct i2c_client *client);
int atsha204_i2c_transmit(const struct i2c_client *client,
                          const char __user *buf, size_t len);


void atsha204_set_params(struct atsha204_cmd_metadata *cmd,
                         int expected_rec_len,
                         unsigned long usleep)
{
    cmd->expected_rec_len = expected_rec_len;
    cmd->usleep = usleep;
}
