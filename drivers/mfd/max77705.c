/*
 * max77705.c - mfd core driver for the Maxim 77705
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77705.h>
#include <linux/mfd/max77705-private.h>

#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_PMIC	(0xCC >> 1)	/* Top sys, Haptic */
#define I2C_ADDR_MUIC	(0x4A >> 1)
#define I2C_ADDR_CHG    (0xD2 >> 1)
#define I2C_ADDR_FG     (0x6C >> 1)
#define I2C_ADDR_DEBUG  (0xC4 >> 1)

static struct mfd_cell max77705_devs[] = {
	{ .name = "leds-max77705-rgb", },
};

int max77705_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77705->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&max77705->i2c_lock);
	if (ret < 0) {
		pr_err("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77705_read_reg);

int max77705_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77705->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77705->i2c_lock);
	if (ret < 0) {
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(max77705_bulk_read);

int max77705_read_word(struct i2c_client *i2c, u8 reg)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77705->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&max77705->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77705_read_word);

int max77705_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77705->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&max77705->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(max77705_write_reg);

int max77705_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77705->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77705->i2c_lock);
	if (ret < 0) {
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(max77705_bulk_write);

int max77705_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77705->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&max77705->i2c_lock);
	if (ret < 0) {
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(max77705_write_word);

int max77705_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int ret;
	u8 old_val, new_val;

	mutex_lock(&max77705->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0) {
		goto err;
	}
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
		if (ret < 0) {
			goto err;
		}
	}
err:
	mutex_unlock(&max77705->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77705_update_reg);

#if defined(CONFIG_OF)
static int of_max77705_dt(struct device *dev, struct max77705_platform_data *pdata)
{
	struct device_node *np_max77705 = dev->of_node;
	struct device_node *np_battery;
	int ret = 0;

	if (!np_max77705)
		return -EINVAL;

	np_battery = of_find_node_by_name(NULL, "battery");
	if (!np_battery) {
		pr_info("%s: np_battery NULL\n", __func__);
	} else {
		pdata->wpc_en = of_get_named_gpio(np_battery, "battery,wpc_en", 0);
		if (pdata->wpc_en < 0) {
			pr_info("%s: can't get wpc_en (%d)\n", __func__, pdata->wpc_en);
			pdata->wpc_en = 0;
		}
		ret = of_property_read_string(np_battery,
				"battery,wireless_charger_name", (char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Wireless charger name is Empty\n", __func__);
	}

	return 0;
}
#endif /* CONFIG_OF */

static int max77705_i2c_probe(struct i2c_client *i2c)
{
	struct max77705_dev *max77705;
	struct max77705_platform_data *pdata = i2c->dev.platform_data;

	u8 reg_data;
	int ret = 0;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	max77705 = kzalloc(sizeof(struct max77705_dev), GFP_KERNEL);
	if (!max77705)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct max77705_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err;
		}

		ret = of_max77705_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	max77705->dev = &i2c->dev;
	max77705->i2c = i2c;
	max77705->irq = i2c->irq;
	if (pdata) {
		max77705->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, MAX77705_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
					MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			max77705->irq_base = pdata->irq_base;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&max77705->i2c_lock);

	i2c_set_clientdata(i2c, max77705);

	if (max77705_read_reg(i2c, MAX77705_PMIC_REG_PMICREV, &reg_data) < 0) {
		dev_err(max77705->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else {
		/* print rev */
		max77705->pmic_rev = (reg_data & 0x7);
		max77705->pmic_ver = ((reg_data & 0xF8) >> 0x3);
		pr_info("%s:%s device found: rev.0x%x, ver.0x%x\n",
				MFD_DEV_NAME, __func__,
				max77705->pmic_rev, max77705->pmic_ver);
	}

	/* No active discharge on safeout ldo 1,2 */
	/* max77705_update_reg(i2c, MAX77705_PMIC_REG_SAFEOUT_CTRL, 0x00, 0x30); */

	init_waitqueue_head(&max77705->queue_empty_wait_q);
	max77705->muic = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_MUIC);
	i2c_set_clientdata(max77705->muic, max77705);

	max77705->charger = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_CHG);
	i2c_set_clientdata(max77705->charger, max77705);

	max77705->fuelgauge = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_FG);
	i2c_set_clientdata(max77705->fuelgauge, max77705);

	max77705->debug = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_DEBUG);
	i2c_set_clientdata(max77705->debug, max77705);

	disable_irq(max77705->irq);
//	ret = max77705_irq_init(max77705);

//	if (ret < 0)
//		goto err_irq_init;

	ret = mfd_add_devices(max77705->dev, -1, max77705_devs,
			ARRAY_SIZE(max77705_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(max77705->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(max77705->dev);
// err_irq_init:
	i2c_unregister_device(max77705->muic);
err_w_lock:
	mutex_destroy(&max77705->i2c_lock);
err:
	kfree(max77705);
	return ret;
}

static void max77705_i2c_remove(struct i2c_client *i2c)
{
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);

	device_init_wakeup(max77705->dev, 0);
	mfd_remove_devices(max77705->dev);
	i2c_unregister_device(max77705->muic);
	kfree(max77705);
}

static const struct i2c_device_id max77705_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_MAX77705 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77705_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id max77705_i2c_dt_ids[] = {
	{ .compatible = "maxim,max77705" },
	{ },
};
MODULE_DEVICE_TABLE(of, max77705_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int max77705_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);

	pr_info("%s:%s +\n", MFD_DEV_NAME, __func__);

	if (device_may_wakeup(dev))
		enable_irq_wake(max77705->irq);
	
	wait_event_interruptible_timeout(max77705->queue_empty_wait_q,
					(!max77705->doing_irq) && (!max77705->is_usbc_queue), 1*HZ);

	pr_info("%s:%s -\n", MFD_DEV_NAME, __func__);
	
	disable_irq(max77705->irq);

	return 0;
}

static int max77705_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	if (device_may_wakeup(dev))
		disable_irq_wake(max77705->irq);

	enable_irq(max77705->irq);

	return 0;
}
#else
#define max77705_suspend	NULL
#define max77705_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION

u8 max77705_dumpaddr_muic[] = {
	MAX77705_MUIC_REG_INTMASK_MAIN,
	MAX77705_MUIC_REG_INTMASK_BC,
	MAX77705_MUIC_REG_INTMASK_FC,
	MAX77705_MUIC_REG_INTMASK_GP,
	MAX77705_MUIC_REG_STATUS1_BC,
	MAX77705_MUIC_REG_STATUS2_BC,
	MAX77705_MUIC_REG_STATUS_GP,
	MAX77705_MUIC_REG_CONTROL1_BC,
	MAX77705_MUIC_REG_CONTROL2_BC,
	MAX77705_MUIC_REG_CONTROL1,
	MAX77705_MUIC_REG_CONTROL2,
	MAX77705_MUIC_REG_CONTROL3,
	MAX77705_MUIC_REG_CONTROL4,
	MAX77705_MUIC_REG_HVCONTROL1,
	MAX77705_MUIC_REG_HVCONTROL2,
};

u8 max77705_dumpaddr_led[] = {
	MAX77705_RGBLED_REG_LEDEN,
	MAX77705_RGBLED_REG_LED0BRT,
	MAX77705_RGBLED_REG_LED1BRT,
	MAX77705_RGBLED_REG_LED2BRT,
	MAX77705_RGBLED_REG_LED3BRT,
	MAX77705_RGBLED_REG_LEDBLNK,
	MAX77705_RGBLED_REG_LEDRMP,
};

static int max77705_freeze(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < ARRAY_SIZE(max77705_dumpaddr_pmic); i++)
		max77705_read_reg(i2c, max77705_dumpaddr_pmic[i],
				&max77705->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77705_dumpaddr_muic); i++)
		max77705_read_reg(i2c, max77705_dumpaddr_muic[i],
				&max77705->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77705_dumpaddr_led); i++)
		max77705_read_reg(i2c, max77705_dumpaddr_led[i],
				&max77705->reg_led_dump[i]);

	disable_irq(max77705->irq);

	return 0;
}

static int max77705_restore(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77705_dev *max77705 = i2c_get_clientdata(i2c);
	int i;

	enable_irq(max77705->irq);

	for (i = 0; i < ARRAY_SIZE(max77705_dumpaddr_pmic); i++)
		max77705_write_reg(i2c, max77705_dumpaddr_pmic[i],
				max77705->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77705_dumpaddr_muic); i++)
		max77705_write_reg(i2c, max77705_dumpaddr_muic[i],
				max77705->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77705_dumpaddr_led); i++)
		max77705_write_reg(i2c, max77705_dumpaddr_led[i],
				max77705->reg_led_dump[i]);

	return 0;
}
#endif

const struct dev_pm_ops max77705_pm = {
	.suspend = max77705_suspend,
	.resume = max77705_resume,
#ifdef CONFIG_HIBERNATION
	.freeze =  max77705_freeze,
	.thaw = max77705_restore,
	.restore = max77705_restore,
#endif
};

static struct i2c_driver max77705_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &max77705_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= max77705_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= max77705_i2c_probe,
	.remove		= max77705_i2c_remove,
	.id_table	= max77705_i2c_id,
};

static int __init max77705_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&max77705_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(max77705_i2c_init);

static void __exit max77705_i2c_exit(void)
{
	i2c_del_driver(&max77705_i2c_driver);
}
module_exit(max77705_i2c_exit);

MODULE_DESCRIPTION("MAXIM 77705 multi-function core driver");
MODULE_AUTHOR("Insun Choi <insun77.choi@samsung.com>");
MODULE_LICENSE("GPL");
