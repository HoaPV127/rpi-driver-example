/**
 * @file blink-led.c
 * @author HoaPV (hoa.pv127@gmail.com)
 * @brief driver for raspi 3b+ to control blink a led
 * @version 0.1
 * @date 2021-06-06
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <linux/module.h>       /* for macro module_init, mudule_exit */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>        /**/
#include <linux/fs.h>           /*  for alloc_chardev_region() */
#include <linux/device.h>       /*  class_create() device_create() */
#include <linux/cdev.h>         /**/
#include <linux/slab.h>         /* kmaloc, kfree */
#include <linux/uaccess.h>      /**/
#include <asm/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/ioport.h>

#define DRIVER_AUTHOR   "HoaPV - hoa.pv127@gmail.com"
#define DRIVER_DESC     "this driver controls blinking a led"
#define DRIVER_VERSION  "0.1"

#define STATUS_LEN  16
#define GPIO_BASE   0x3F200000
#define BLOCKSIZE   4096
// #define LED         (29)

#define CLEAR_REG(reg)  *(volatile unsigned int *)(reg) &= (unsigned int)0x00000000
#define LED_INPUT(b)    *(volatile unsigned int *)(0x00000000+b) &= (unsigned int) 0xFFFF8FFF
#define LED_OUTPUT(b)   *(volatile unsigned int *)(0x00000000+b) |= (unsigned int) 0x00001000

#define SET_LED(b)      *(volatile unsigned int *)(0x0000001C+b) |= 0x10
#define CLEAR_LED(b)    *(volatile unsigned int *)(0x00000028+b) |= 0x10

volatile unsigned int * gpio = NULL;

struct _blink_led_dev_t {
    unsigned char * status; 
};

struct _blink_led_drv {
    dev_t   dev_num;
    struct class *dev_class;
    struct device *dev;
    struct cdev *led_cdev;
    struct _blink_led_dev_t *blink_led_data;
} blink_led_drv;

static int blink_led_dev_init(struct _blink_led_dev_t *hw);
static void blink_led_dev_free(struct _blink_led_dev_t *hw);

static int blink_led_open(struct inode *inode, struct file *filp);
static int blink_led_release(struct inode *inode, struct file *filp);
static ssize_t blink_led_read(struct file *filp, char __user *buff, size_t len, loff_t *off);
static ssize_t blink_led_write(struct file *filp, const char __user *buff, size_t len, loff_t *off);

static struct file_operations fops = 
{
    .owner      = THIS_MODULE,
    .open       = blink_led_open,
    .release    = blink_led_release,
    .read       = blink_led_read,
    .write      = blink_led_write,
};

static int blink_led_dev_init(struct _blink_led_dev_t *hw) {
    hw->status = kzalloc(STATUS_LEN, GFP_KERNEL);
    if(!hw->status) {
        return -ENOMEM;
    }

    return 0;
}

static void blink_led_dev_free(struct _blink_led_dev_t *hw) {
    if(hw->status) {
        kfree(hw->status);
    }
}

static int blink_led_open(struct inode *inode, struct file *filp) {
    pr_info("Handle opened event\n");

    return 0;
}

static int blink_led_release(struct inode *inode, struct file *filp) {
    pr_info("Handle released event\n");

    return 0;
}

static ssize_t blink_led_read(struct file *filp, char __user *buff, size_t len, loff_t *off) {
    int ret = 0;
    // int bytes = 0;


    pr_info("Handle read event start from %lld, %zu bytes\n", *off, len);

    if(*off >= STATUS_LEN) {
        return 0;
    }
    
    if(*off + len >= STATUS_LEN) {
        len = STATUS_LEN - (*off);
    }

    // bytes = strlen(blink_led_drv.blink_led_data->status);
    ret = copy_to_user(buff, blink_led_drv.blink_led_data->status, STATUS_LEN);
    if(ret < 0) {
        return -EFAULT;
    }

    *off += STATUS_LEN;

    return STATUS_LEN;
}

static ssize_t blink_led_write(struct file *filp, const char __user *buff, size_t len, loff_t *off) {
    char *kernel_buff = NULL;
    int ret = 0;

    pr_info("Hanle write envent start from %lld, %zu bytes\n", *off, len);

    kernel_buff = kzalloc(len, GFP_KERNEL);
    ret = copy_from_user(kernel_buff, buff, len);
    if(ret < 0) {
        return -EFAULT;
    }

    if(!strncmp((const char *)kernel_buff, "start", strlen("start"))) {
        strcpy(blink_led_drv.blink_led_data->status, "start");

        LED_OUTPUT(gpio);
        SET_LED(gpio);
    }
    else if(!strncmp((const char *)kernel_buff, "stop", strlen("stop"))) {
        strcpy(blink_led_drv.blink_led_data->status, "stop");
        
        LED_OUTPUT(gpio);
        CLEAR_LED(gpio);
    }
    else {
        pr_err("%s: Wrong command\n", kernel_buff);
    }

    *off += len;

    return len;
}

/*  init driver */
static int __init blink_led_drv_init(void) {
    int ret = 0;

    /*  Allocate deivce number */
    ret = alloc_chrdev_region(&blink_led_drv.dev_num, 0, 1, "blink-led");
    if(ret < 0) {
        pr_err("Cannot allocate device number\n");

        goto failed_alloc_device_number;
    }

    /*  Create device file */
    blink_led_drv.dev_class = class_create(THIS_MODULE, "class-blink-led");
    if(blink_led_drv.dev_class == NULL) {
        pr_err("Cannot create a device class\n");
        
        goto failed_create_class_device;
    }

    blink_led_drv.dev = device_create(blink_led_drv.dev_class, NULL, blink_led_drv.dev_num, NULL, "blink-led");
    if(IS_ERR(blink_led_drv.dev)) {
        pr_err("Cannot create a device\n");

        goto failed_create_device;
    } 

    blink_led_drv.blink_led_data = kzalloc(sizeof(struct _blink_led_dev_t), GFP_KERNEL);
    if(!blink_led_drv.blink_led_data) {
        pr_err("cannot allocate data structure of the driver\n");
        ret = -ENOMEM;
        
        goto failed_alloc_data_struct;
    }

    ret = blink_led_dev_init(blink_led_drv.blink_led_data);
    if(ret < 0) {
        pr_err("Cannot init blink-led device\n");

        goto failed_init_blink_led_device; 
    }

    blink_led_drv.led_cdev = cdev_alloc();
    if(blink_led_drv.led_cdev == NULL) {
        pr_err("Cannot allocate cdev structure\n");
        goto failed_alloc_cdev;
    }

    cdev_init(blink_led_drv.led_cdev, &fops);
    ret = cdev_add(blink_led_drv.led_cdev, blink_led_drv.dev_num, 1);
    if(ret < 0) {
        pr_err("Cannot add a character device to the system\n");
        goto failed_alloc_cdev;
    }
    
    pr_info("PHYSICAL ADDRESS = 0x%X\n", (GPIO_BASE + 8));
    msleep(100);
    gpio = ioremap(GPIO_BASE, BLOCKSIZE);
    if(gpio == NULL) {
        pr_err("Cannot map GPIO\n");
        goto failed_gpio;   
    }
    pr_info("VIRTUAL ADDRESS = 0x%X\n", gpio + 8);
    msleep(100);

    // CLEAR_REG(gpio + 0x8);
    // CLEAR_REG(gpio + 0x1C);
    // CLEAR_REG(gpio + 0x28);
    LED_INPUT(gpio);
    msleep(100);
    pr_info("SET gpio as input\n");
    LED_OUTPUT(gpio);
    msleep(100);
    pr_info("SET gpio as output\n");
    SET_LED(gpio);
    msleep(100);
    pr_info("SET gpio output at high level\n");

    pr_info("value of 0x0 = 0x%X\n", *(gpio));

    strncpy(blink_led_drv.blink_led_data->status, "stop", strlen("stop"));
    pr_info("Initialize blink led driver successfully\n");
    return 0;

failed_gpio:
    cdev_del(blink_led_drv.led_cdev);

failed_alloc_cdev:
    blink_led_dev_free(blink_led_drv.blink_led_data);

failed_init_blink_led_device:
    kfree(blink_led_drv.blink_led_data);

failed_alloc_data_struct:
    device_destroy(blink_led_drv.dev_class, blink_led_drv.dev_num);

failed_create_device:
    class_destroy(blink_led_drv.dev_class);

failed_create_class_device:
    unregister_chrdev_region(blink_led_drv.dev_num, 1);

failed_alloc_device_number:
    return ret;
}

/*  remove driver */
static void __exit blink_led_drv_exit(void) {

    // gpio_free(LED);
    iounmap(gpio);
    cdev_del(blink_led_drv.led_cdev);

    blink_led_dev_free(blink_led_drv.blink_led_data);

    kfree(blink_led_drv.blink_led_data);

    /*  remove device file */
    device_destroy(blink_led_drv.dev_class, blink_led_drv.dev_num);
    class_destroy(blink_led_drv.dev_class);

    /*  unregister device number */
    unregister_chrdev_region(blink_led_drv.dev_num, 1);

    pr_info("Exit blink led driver\n");
}

module_init(blink_led_drv_init);
module_exit(blink_led_drv_exit);

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);