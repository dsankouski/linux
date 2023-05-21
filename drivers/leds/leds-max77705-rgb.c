/*
 * RGB-led driver for Maxim MAX77705
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DRIVER_NAME		"leds-max77705-rgb"
#define pr_fmt(fmt)		DRIVER_NAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/mfd/max77705.h>
#include <linux/mfd/max77705-private.h>

/* Registers */

/* MAX77705_REG_LED0BRT */
#define MAX77705_LED0BRT	0xFF

/* MAX77705_REG_LED1BRT */
#define MAX77705_LED1BRT	0xFF

/* MAX77705_REG_LED2BRT */
#define MAX77705_LED2BRT	0xFF

/* MAX77705_REG_LED3BRT */
#define MAX77705_LED3BRT	0xFF

/* MAX77705_REG_LEDBLNK */
#define MAX77705_LEDBLINKD	0xF0
#define MAX77705_LEDBLINKP	0x0F

/* MAX77705_REG_LEDRMP */
#define MAX77705_RAMPUP		0xF0
#define MAX77705_RAMPDN		0x0F

#define LED_R_MASK		0x00FF0000
#define LED_G_MASK		0x0000FF00
#define LED_B_MASK		0x000000FF
#define LED_MAX_CURRENT		0xFF

/* MAX77705_STATE*/
#define LED_DISABLE			0
#define LED_ALWAYS_ON			1
#define LED_BLINK			2

#define LEDBLNK_ON(time)	((time < 100) ? 0 :			\
				(time < 500) ? time/100-1 :		\
				(time < 3250) ? (time-500)/250+4 : 15)

#define LEDBLNK_OFF(time)	((time < 1) ? 0x00 :			\
				(time < 500) ? 0x01 :			\
				(time < 5000) ? time/500 :		\
				(time < 8000) ? (time-5000)/1000+10 :	 \
				(time < 12000) ? (time-8000)/2000+13 : 15)

#define RGB_BUFSIZE		30

enum max77705_led_color {
	WHITE,
	RED,
	GREEN,
	BLUE,
};

struct max77705_rgb {
	struct led_classdev led[4];
	struct i2c_client *i2c;
};

static int max77705_rgb_number(struct led_classdev *led_cdev,
				struct max77705_rgb **p)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77705_rgb *max77705_rgb = dev_get_drvdata(parent);
	int i;

	*p = max77705_rgb;

	for (i = 0; i < 4; i++) {
		if (led_cdev == &max77705_rgb->led[i]) {
			pr_debug("leds-max77705-rgb: %s, %d\n", __func__, i);
			return i;
		}
	}

	pr_err("leds-max77705-rgb: %s, can't find rgb number\n", __func__);

	return -ENODEV;
}

static void max77705_rgb_set(struct led_classdev *led_cdev,
				unsigned int brightness)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77705_rgb *max77705_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;

	ret = max77705_rgb_number(led_cdev, &max77705_rgb);
	if (ret < 0) {
		dev_err(led_cdev->dev,
			"max77705_rgb_number() returns %d.\n", ret);
		return;
	}

	dev = led_cdev->dev;
	n = ret;

	if (brightness == LED_OFF) {
		/* Flash OFF */
		ret = max77705_update_reg(max77705_rgb->i2c,
					MAX77705_RGBLED_REG_LEDEN, 0, 3 << (2*n));
		if (ret < 0) {
			dev_err(dev, "can't write LEDEN : %d\n", ret);
			return;
		}
	} else {
		/* Set current */
		ret = max77705_write_reg(max77705_rgb->i2c,
				MAX77705_RGBLED_REG_LED0BRT + n, brightness);
		if (ret < 0) {
			dev_err(dev, "can't write LEDxBRT : %d\n", ret);
			return;
		}
		ret = max77705_update_reg(max77705_rgb->i2c,
				MAX77705_RGBLED_REG_LEDEN, LED_ON << (2*n), 0x3 << 2*n);
		if (ret < 0) {
			dev_err(dev, "can't write LEDEN : %d\n", ret);
			return;
		}
	}
}

static unsigned int max77705_rgb_get(struct led_classdev *led_cdev)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77705_rgb *max77705_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;
	u8 value;

	pr_info("leds-max77705-rgb: %s\n", __func__);

	ret = max77705_rgb_number(led_cdev, &max77705_rgb);
	if (ret < 0) {
		dev_err(led_cdev->dev,
			"max77705_rgb_number() returns %d.\n", ret);
		return 0;
	}
	n = ret;

	dev = led_cdev->dev;

	/* Get status */
	ret = max77705_read_reg(max77705_rgb->i2c,
				MAX77705_RGBLED_REG_LEDEN, &value);
	if (ret < 0) {
		dev_err(dev, "can't read LEDEN : %d\n", ret);
		return 0;
	}
	if (!(value & (3 << (2*n))))
		return LED_OFF;

	/* Get current */
	ret = max77705_read_reg(max77705_rgb->i2c,
				MAX77705_RGBLED_REG_LED0BRT + n, &value);
	if (ret < 0) {
		dev_err(dev, "can't read LED0BRT : %d\n", ret);
		return 0;
	}

	return value;
}

#ifdef CONFIG_OF
static int max77705_rgb_parse_dt(struct device *dev, struct max77705_rgb *max77705_rgb)
{
	struct device_node *np;
	char *function, *led_name;
	int function_len;
	int ret;
	int i;

	pr_info("leds-max77705-rgb: %s\n", __func__);

	np = of_find_node_by_name(dev->parent->of_node, "leds");
	if (unlikely(np == NULL)) {
		dev_err(dev, "leds node not found\n");
		return -EINVAL;
	}

	ret = of_property_read_string_index(np, "function", i,
					(const char **)&function);
	if (ret < 0)
		return ret;
	function_len = strlen(function);

	led_name = devm_kzalloc(dev, function_len + 7, GFP_KERNEL);
	snprintf(led_name, function_len + 7, "white:%s", function);
	max77705_rgb->led[WHITE].name = led_name;

	led_name = devm_kzalloc(dev, function_len + 5, GFP_KERNEL);
	snprintf(led_name, function_len + 5, "red:%s", function);
	max77705_rgb->led[RED].name = led_name;

	led_name = devm_kzalloc(dev, function_len + 7, GFP_KERNEL);
	snprintf(led_name, function_len + 7, "green:%s", function);
	max77705_rgb->led[GREEN].name = led_name;

	led_name = devm_kzalloc(dev, function_len + 6, GFP_KERNEL);
	snprintf(led_name, function_len + 6, "blue:%s", function);
	max77705_rgb->led[BLUE].name = led_name;

	return 0;
}
#endif

static void max77705_rgb_reset(struct device *dev)
{
	struct max77705_rgb *max77705_rgb = dev_get_drvdata(dev);

	max77705_rgb_set(&max77705_rgb->led[RED], 0);
	max77705_rgb_set(&max77705_rgb->led[GREEN], 0);
	max77705_rgb_set(&max77705_rgb->led[BLUE], 0);
}

static int max77705_rgb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77705_rgb *max77705_rgb;
	struct max77705_dev *max77705_dev = dev_get_drvdata(dev->parent);
	int i, ret;

	pr_info("leds-max77705-rgb: %s\n", __func__);

	max77705_rgb = devm_kzalloc(dev, sizeof(struct max77705_rgb), GFP_KERNEL);
	if (unlikely(!max77705_rgb))
		return -ENOMEM;

#ifdef CONFIG_OF
	ret = max77705_rgb_parse_dt(dev, max77705_rgb);
	if (ret < 0)
		return ret;
#endif

	max77705_rgb->i2c = max77705_dev->i2c;
	platform_set_drvdata(pdev, max77705_rgb);

	for (i = 0; i < 4; i++) {
		max77705_rgb->led[i].brightness_set = max77705_rgb_set;
		max77705_rgb->led[i].brightness_get = max77705_rgb_get;
		max77705_rgb->led[i].max_brightness = LED_MAX_CURRENT;

		ret = led_classdev_register(dev, &max77705_rgb->led[i]);
		if (ret < 0) {
			dev_err(dev, "unable to register RGB : %d\n", ret);
			goto alloc_err;
		}
	}

	pr_info("leds-max77705-rgb: %s done\n", __func__);

	return 0;

alloc_err:
	while (i--)
		led_classdev_unregister(&max77705_rgb->led[i]);

	devm_kfree(dev, max77705_rgb);
	return -ENOMEM;
}

static int max77705_rgb_remove(struct platform_device *pdev)
{
	struct max77705_rgb *max77705_rgb = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < 4; i++)
		led_classdev_unregister(&max77705_rgb->led[i]);

	return 0;
}

static void max77705_rgb_shutdown(struct platform_device *pdev)
{
	struct max77705_rgb *max77705_rgb = platform_get_drvdata(pdev);
	int i;

	if (!max77705_rgb->i2c)
		return;

	max77705_rgb_reset(&pdev->dev);

	for (i = 0; i < 4; i++)
		led_classdev_unregister(&max77705_rgb->led[i]);
	devm_kfree(&pdev->dev, max77705_rgb);
}

static struct platform_driver max77705_fled_driver = {
	.driver		= {
		.name	= "leds-max77705-rgb",
		.owner	= THIS_MODULE,
	},
	.probe		= max77705_rgb_probe,
	.remove		= max77705_rgb_remove,
	.shutdown	= max77705_rgb_shutdown,
};

static int __init max77705_rgb_init(void)
{
	pr_info("leds-max77705-rgb: %s\n", __func__);
	return platform_driver_register(&max77705_fled_driver);
}
module_init(max77705_rgb_init);

static void __exit max77705_rgb_exit(void)
{
	platform_driver_unregister(&max77705_fled_driver);
}
module_exit(max77705_rgb_exit);

MODULE_ALIAS("platform:max77705-rgb");
MODULE_AUTHOR("Jeongwoong Lee<jell.lee@samsung.com>");
MODULE_DESCRIPTION("MAX77705 RGB driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
