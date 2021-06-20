#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/printk.h>

// struct platform_device ex_device;

static int ex_probe(struct platform_device *pdev)
{
	struct resource *regs = NULL;
    pr_info("probe function\n");

    regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);

    if(regs == NULL) {
        pr_err("get resource failed\n");
    }
    else {
        pr_info("BASE-ADDR: 0x%X\n", regs->start);
    }

	return 0;
}

static void ex_remove(struct platform_device *pdev) {
    pr_info("remove function\n");
}   

static const struct of_device_id of_leds_match[] = {
	{ .compatible = "example-gpio", },
	{},
};

static struct platform_driver led_driver = {
	.probe		= ex_probe,
	.shutdown	= ex_remove,
	.driver		= {
		.name	= "example-drv",
        .owner  = THIS_MODULE,
        .of_match_table = of_leds_match,
	},
};

module_platform_driver(led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hoapv");
MODULE_DESCRIPTION("gpio example");