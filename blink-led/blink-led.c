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
// #include <linux/gpio.h>
// #include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#define DRIVER_AUTHOR   "HoaPV - hoa.pv127@gmail.com"
#define DRIVER_DESC     "this driver controls blinking a led"
#define DRIVER_VERSION  "0.1"

#define STATUS_LEN  16
#define GPIO_BASE   0x3F200000      /* start of GPIO address */
#define BLOCKSIZE   0xB4            /* size of GPIO address */

/*  these definiton below are defined for GPIO_29 */
#define GPIO_INPUT(b)   *(volatile unsigned int *)(0x00000008 + (volatile void *)b) &= (unsigned int) 0xC7FFFFFF
#define GPIO_OUTPUT(b)  *(volatile unsigned int *)(0x00000008 + (volatile void *)b) &= (unsigned int) 0xC7FFFFFF; \
                        *(volatile unsigned int *)(0x00000008 + (volatile void *)b) |= (unsigned int) 0x08000000

#define GPIO_SET(b)     *(volatile unsigned int *)(0x0000001C + (volatile void *)b) |= 0x20000000
#define GPIO_CLR(b)     *(volatile unsigned int *)(0x00000028 + (volatile void *)b) |= 0x20000000

#define LED_ON(b)       GPIO_SET(b)
#define LED_OFF(b)      GPIO_CLR(b)

volatile void *gpio = NULL;

struct _blink_led_dev_t {
    struct task_struct *ts;  /* Task handle to identify thread */
    unsigned int freq;              /* blinking frequency */
    unsigned char *status;          /* status */
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
static int blinking_thread(void *data); 

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
    hw->freq = 1;
    hw->ts = NULL;
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
    unsigned int read_size = 0;
    char *kernel_buff = NULL;

    pr_info("Handle read event start from %lld, %zu bytes\n", *off, len);

    kernel_buff = kzalloc(1024, GFP_KERNEL);
    if(kernel_buff == NULL) {
        pr_err("Cannot allocate kernel memory\n");
        return -ENOMEM;
    }

    sprintf(kernel_buff,    "status: %16s\n"
                            "freq:   %16u\n"
                            "\n********** HELP **********\n"
                            "- write \"start\" to start blinking\n"
                            "- write \"stop\" to stop blinking\n"
                            "- write \"freq x\" to change blinking frequency, with x is frequency\n",
                            blink_led_drv.blink_led_data->status,
                            blink_led_drv.blink_led_data->freq);

    read_size = strlen(kernel_buff);

    if(*off >= read_size) {
        return 0;
    }
    
    if(*off + len >= read_size) {
        len = read_size - (*off);
    }

    ret = copy_to_user(buff, kernel_buff, read_size);
    if(ret < 0) {
        return -EFAULT;
    }

    *off += read_size;

    return read_size;
}

static ssize_t blink_led_write(struct file *filp, const char __user *buff, size_t len, loff_t *off) {
    char *kernel_buff = NULL;
    int ret = 0;

    pr_info("Hanle write envent start from %lld, %zu bytes\n", *off, len);

    kernel_buff = kzalloc(len, GFP_KERNEL);
    if(kernel_buff == NULL) {
        pr_err("Cannot allocate kmem\n");
        return -ENOMEM;
    }

    ret = copy_from_user(kernel_buff, buff, len);
    if(ret < 0) {
        pr_err("Cannot copy from user\n");
        kfree(kernel_buff);
        return -EFAULT;
    }

    if(!strncmp((const char *)kernel_buff, "start", strlen("start"))) {
        blink_led_drv.blink_led_data->ts = kthread_create(blinking_thread, NULL, "blink-led");
        if(blink_led_drv.blink_led_data->ts) {
            wake_up_process(blink_led_drv.blink_led_data->ts);
            strcpy(blink_led_drv.blink_led_data->status, "start");
        }
        else {
            pr_err("Cannot create blinking thread\n");
        }
    }
    else if(!strncmp((const char *)kernel_buff, "stop", strlen("stop"))) {

        if(blink_led_drv.blink_led_data->ts) {
            strcpy(blink_led_drv.blink_led_data->status, "stop");
            kthread_stop(blink_led_drv.blink_led_data->ts);
            blink_led_drv.blink_led_data->ts = NULL;
            LED_OFF(gpio);
        }
        else {
            pr_err("Blinking thread is not started yet\n");
        }
    }
    else if(!strncmp((const char *)kernel_buff, "freq ", strlen("freq "))) {
        sscanf((const char *)kernel_buff, "freq %d", &(blink_led_drv.blink_led_data->freq));
        pr_info("Change Freq %u\n", blink_led_drv.blink_led_data->freq);
    }
    else {
        pr_err("%s: Wrong command\n", kernel_buff);
    }

    *off += len;

    return len;
}

static int blinking_thread(void *data) {
    pr_info("Starting thread to blink a led\n");

    while(!kthread_should_stop()) {
        unsigned int udelay = (unsigned int)(1000 / blink_led_drv.blink_led_data->freq);
        if(udelay < 1) {
            udelay = 1;
        }

        LED_ON(gpio);
        msleep(udelay);

        LED_OFF(gpio);
        msleep(udelay);
    }

    return 0;
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
    
    gpio = ioremap(GPIO_BASE, BLOCKSIZE);
    if(gpio == NULL) {
        pr_err("Cannot map GPIO\n");
        goto failed_gpio;   
    }

    GPIO_INPUT(gpio);
    GPIO_OUTPUT(gpio);
    LED_OFF(gpio);

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

    if(blink_led_drv.blink_led_data->ts) {
        kthread_stop(blink_led_drv.blink_led_data->ts);
        blink_led_drv.blink_led_data->ts = NULL;
    }
    LED_OFF(gpio);
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