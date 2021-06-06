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

#define DRIVER_AUTHOR   "HoaPV - hoa.pv127@gmail.com"
#define DRIVER_DESC     "this driver controls blinking a led"
#define DRIVER_VERSION  "0.1"

struct _blink_led_dev_t {
    unsigned char * status; 
};

struct _blink_led_drv {
    dev_t   dev_num;
    struct class *dev_class;
    struct device *dev;
    struct _blink_led_dev_t *blink_led_data;
} blink_led_drv;

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

    pr_info("Initialize blink led driver successfully\n");
    return 0;

failed_create_device:
    class_destroy(blink_led_drv.dev_class);

failed_create_class_device:
    unregister_chrdev_region(blink_led_drv.dev_num, 1);

failed_alloc_device_number:
    return ret;
}

/*  remove driver */
static void __exit blink_led_drv_exit(void) {

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