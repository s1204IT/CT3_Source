/*
 *  lc709203f_battery.c
 *  Battery Monitor IC for 1 cell Li+ battery
 *
 *  Copyright (C) 2014-2015 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
/*
 * Changelog:
 *
 * 2018-Feb     Add driver for the battery monitor.
 *
 * 2018-Feb	Takuya Yata <yata_takuya@sts-tottori.com> Changed
 *		wake up by interrupt during suspend,
 *		interrupt setting when <20% and <1%
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */
#include "lc709203f.h"
#include <mt-plat/battery_meter_hal.h>

#define N_AR(x) (sizeof(x)/sizeof(x[0]))

#define LC709203F_THERMISTOR_B		0x06
#define LC709203F_INITIAL_RSOC		0x07
#define LC709203F_CELL_TEMPERATURE	0x08
#define LC709203F_CELL_VOLTAGE		0x09
#define LC709203F_ADJUSTMENT_PACK_APPLI	0x0B
#define LC709203F_ADJUSTMENT_PACK_THERM	0x0C
#define LC709203F_RSOC			0x0D
#define LC709203F_INDICATOR_TO_EMPTY	0x0F
#define LC709203F_IC_VERSION		0x11
#define LC709203F_CHANGE_OF_THE_PARAM	0x12
#define LC709203F_ALARM_LOW_CELL_RSOC	0x13
#define LC709203F_ALARM_LOW_CELL_VOLT	0x14
#define LC709203F_IC_POWER_MODE		0x15
#define LC709203F_STATUS_BIT		0x16
#define LC709203F_NUM_OF_THE_PARAM	0x1A

#define RESCALE_SOC
#define INDI_TOP			950
#define INDI_BOTTOM			100
#define INDI_BOTTOM_HICHARGE 50

#define EMPTYSOC  10  /*  1% */
#define EMPTYSOC_HICHARGE  5  /*  1% */

static struct i2c_client *new_client = NULL;

static struct wake_lock lc709203f_suspend_lock;

static int lc709203f_set_irq_handler(const struct i2c_client *client);
static int lc709203f_get_alarm_low_rsoc(void);

static u8 lc709203f_crc8(u8 before, u8 after)
{
	int ii;
	u16 TmpValue = 0;

	TmpValue = (u16)(before ^ after);
	TmpValue <<= 8;

	for( ii = 0 ; ii < 8 ; ii++) {
		if( TmpValue & 0x8000) {
			TmpValue ^= 0x8380;
		}
		TmpValue <<= 1;
	}

	return TmpValue >> 8;
}

static int lc709203f_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	s32 ret;
	u8 buf[3];
	u8 crc8;

	buf[0] = value & 0x00ff;
	buf[1] = value >> 8;
	crc8 = lc709203f_crc8( 0, 0x16);
	crc8 = lc709203f_crc8( crc8, reg);
	crc8 = lc709203f_crc8( crc8, buf[0]);
	crc8 = lc709203f_crc8( crc8, buf[1]);
	buf[2] = crc8;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 3 ,buf);
//	printk("[lc709203f] %s (0x%02X) <= 0x%04X #%hu ret=%d\n", __FUNCTION__, reg, value, value, ret);

	if (ret < 0) {
		dev_err(&client->dev, "[%s] err: %d\n", __func__, ret);
	}

	return ret;
}

static int lc709203f_read_reg(struct i2c_client *client, u8 reg, u16 *value)
{
	u8 buf[3];
	u8 crc8;
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 3, buf);
	//printk("[lc709203f] %s (0x%02X) => 0x%04X #%hu ret=%d\n", __FUNCTION__, reg, *(u16*)buf, *(u16*)buf, ret);

	if (ret < 0) {
		dev_err(&client->dev, "[%s] err: %d\n", __func__, ret);
		return ret;
	}

	crc8 = lc709203f_crc8(0, 0x16);
	crc8 = lc709203f_crc8(crc8, reg);
	crc8 = lc709203f_crc8(crc8, 0x17);
	crc8 = lc709203f_crc8(crc8, buf[0]);
	crc8 = lc709203f_crc8(crc8, buf[1]);

	if (crc8 == buf[2]) {
		*value = ((u16)buf[1] << 8) | buf[0];
	}
	else {
		printk("crc error\n");
		ret = -1;
	}

	return ret;
}
int lc709203f_manage_rsoc_alarm(int rsoc)
{
	int ret = 0;
	int curr;
	int target;

	target = EMPTYSOC;
	curr = lc709203f_get_alarm_low_rsoc();
	if (curr != target) {
		lc709203f_set_alarm_low_rsoc(target);
	}

	return ret;
}

int lc709203f_manage_rsoc_alarm_hicharge(int rsoc)
{
	int ret = 0;
	int curr;
	int target;

	target = EMPTYSOC_HICHARGE;
	curr = lc709203f_get_alarm_low_rsoc();
	if (curr != target) {
		lc709203f_set_alarm_low_rsoc(target);
	}

	return ret;
}

#ifdef RESCALE_SOC
int lc709203f_get_rsoc(void)
{
	struct i2c_client *client = new_client;
	u16 value;
	int ret;

	ret = lc709203f_read_reg(client, LC709203F_INDICATOR_TO_EMPTY, &value);
	if (ret < 0) {
		return INT_MIN;
	}

//	printk("________________________________ LC709203F_RSOC=%d\n", (int)value);

	if (value >= INDI_TOP) {
		value = 100;
	}
	else if (value <= INDI_BOTTOM) {
		value = 0;
	}
	else {
		int denominator = (1000 - INDI_BOTTOM) - (1000 - INDI_TOP);
		value = (int)(value - INDI_BOTTOM) * 1000 / denominator;
		value /= 10; // change scale from 1000 to percentage.
	}

	lc709203f_manage_rsoc_alarm(value);

	return value;
}

int lc709203f_get_rsoc_hicharge(void)
{
	struct i2c_client *client = new_client;
	u16 value;
	int ret;

	ret = lc709203f_read_reg(client, LC709203F_INDICATOR_TO_EMPTY, &value);
	if (ret < 0) {
		return INT_MIN;
	}

//	printk("________________________________ LC709203F_RSOC hicharge=%d\n", (int)value);

	if (value >= INDI_TOP) {
		value = 100;
	}
	else if (value <= INDI_BOTTOM_HICHARGE) {
		value = 0;
	}
	else {
		int denominator = (1000 - INDI_BOTTOM_HICHARGE) - (1000 - INDI_TOP);
		value = (int)(value - INDI_BOTTOM_HICHARGE) * 1000 / denominator;
		value /= 10; // change scale from 1000 to percentage.
	}

	lc709203f_manage_rsoc_alarm_hicharge(value);

	return value;
}
#else
int lc709203f_get_rsoc(void)
{
	struct i2c_client *client = new_client;
	u16 value;
	int ret;

	ret = lc709203f_read_reg(client, LC709203F_RSOC, &value);
	if (ret < 0) {
		return 100;
	}

	return value;
}

int lc709203f_get_rsoc_hicharge(void)
{
	struct i2c_client *client = new_client;
	u16 value;
	int ret;

	ret = lc709203f_read_reg(client, LC709203F_RSOC, &value);
	if (ret < 0) {
		return 100;
	}

	return value;
}
#endif

int lc709203f_get_cell_temperature(void)
{
	struct i2c_client *client = new_client;
	u16 value;
	int temper;
	int ret;

	ret = lc709203f_read_reg(client, LC709203F_CELL_TEMPERATURE, &value);
	if (ret < 0) {
		return INT_MIN;
	}

	temper = ((int)value - 2732)/10;	// convert from Kelvin to Celsius.

	return temper;
}

int lc709203f_get_cell_voltage(void)
{
	struct i2c_client *client = new_client;
	u16 value;
	int ret;

	ret = lc709203f_read_reg(client, LC709203F_CELL_VOLTAGE, &value);
	if (ret < 0) {
		return INT_MIN;
	}

	return value;
}

int lc709203f_get_alarm_low_rsoc(void)
{
	u16 value;
	int ret;
	ret = lc709203f_read_reg(new_client, LC709203F_ALARM_LOW_CELL_RSOC,
				 &value);
	if (ret < 0) {
		return INT_MIN;
	}
	return value;
}


void lc709203f_set_alarm_low_rsoc(u16 value)
{
	struct i2c_client *client = new_client;
	int ret;

	ret = lc709203f_write_reg(client, LC709203F_ALARM_LOW_CELL_RSOC, value);
}

void lc709203f_set_alarm_low_voltage(u16 value)
{
	struct i2c_client *client = new_client;
	int ret;

	ret = lc709203f_write_reg(client, LC709203F_ALARM_LOW_CELL_VOLT, value);
}

void lc709203f_set_ic_power_mode(u16 value)
{
	struct i2c_client *client = new_client;
	int ret;

	ret = lc709203f_write_reg(client, LC709203F_IC_POWER_MODE, value);
}

void lc709203f_set_status_bit(u16 value)
{
	struct i2c_client *client = new_client;
	int ret;

	ret = lc709203f_write_reg(client, LC709203F_STATUS_BIT, value);
}

void lc709203f_dump_register(void)
{
	struct i2c_client *client = new_client;
	static const u8 regs[] = {
		LC709203F_THERMISTOR_B,
		LC709203F_INITIAL_RSOC,
		LC709203F_CELL_TEMPERATURE,
		LC709203F_CELL_VOLTAGE,
		LC709203F_ADJUSTMENT_PACK_APPLI,
		LC709203F_ADJUSTMENT_PACK_THERM,
		LC709203F_RSOC,
		LC709203F_INDICATOR_TO_EMPTY,
		LC709203F_IC_VERSION,
		LC709203F_CHANGE_OF_THE_PARAM,
		LC709203F_ALARM_LOW_CELL_RSOC,
		LC709203F_ALARM_LOW_CELL_VOLT,
		LC709203F_IC_POWER_MODE,
		LC709203F_STATUS_BIT,
		LC709203F_NUM_OF_THE_PARAM
	};
	int i;

	bm_print(BM_LOG_FULL, "[lc709203f]");
	for (i = 0; i < N_AR(regs); i++) {
		u16 value;
		lc709203f_read_reg(client, regs[i], &value);
		bm_print(BM_LOG_FULL, " [0x%02X]=0x%04X", regs[i], value);
	}
	bm_print(BM_LOG_FULL, "\n");
}

static irqreturn_t lc709203f_irq_thread(int irq, void *data)
{
	disable_irq_nosync(irq);

	wake_lock(&lc709203f_suspend_lock);

	return IRQ_RETVAL(0);
}

/**
 *	@brief	get IRQ pin number
 *	@return	if 0 then no IRQ pin assign
 */
static int lc709203f_get_irq_number(const struct device *dev)
{
	int irq_num = 0;

#ifdef CONFIG_OF
	irq_num = of_get_named_gpio(dev->of_node, "irq-gpio", 0);
	pr_notice( "[%s] of_get_named_gpio(): %d\n", __func__, irq_num);

	if (irq_num >= 0) {
		gpio_request(irq_num, NULL);
		gpio_direction_input(irq_num);
		irq_num = gpio_to_irq(irq_num);
	}
#endif /* CONFIG_OF */

	return	irq_num;
}

/**
 *	@brief	set IRQ handler
 *	@return	if !0 then error
 */
static int lc709203f_set_irq_handler(const struct i2c_client *client)
{
	int ret = 0;

	ret = request_threaded_irq(client->irq,
				   NULL,
				   lc709203f_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   client->name,
				   NULL);

	irq_set_irq_wake(client->irq, 1);

	return ret;
}

/**
 *	@brief	probe func. as i2c driver
 */
static int lc709203f_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;
	u16 reg;

	pr_notice("[%s]\n", __FUNCTION__);

	if (i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK) == 0) {
		return -EIO;
	}

	do {
	    ret = lc709203f_read_reg(client, LC709203F_IC_VERSION, &reg);
	    printk("lc709203f_read_reg IC_VERSION\n");
	} while (ret < 0);

	/*battery alart*/
	ret = lc709203f_write_reg(client, LC709203F_ALARM_LOW_CELL_RSOC, EMPTYSOC_HICHARGE);
	if(ret < 0)
	{
		return ret;
	}

	client->irq = lc709203f_get_irq_number(&client->dev);

	new_client = client;

	if (client->irq) {
		printk("[%s] irq %d\n", __func__, client->irq);

		ret = lc709203f_set_irq_handler(client);
		if (ret) {
			dev_err(&client->dev, "Failed to register interrupt\n");
		}
	}
	printk("[%s] version: %04X\n", __func__, reg);

	device_init_wakeup(&client->dev, true);
	wake_lock_init(&lc709203f_suspend_lock, WAKE_LOCK_SUSPEND, "lc709203f suspend wakelock");

	lc709203f_dump_register();

	return 0;
}

static int lc709203f_remove(struct i2c_client *client)
{
	device_init_wakeup(&client->dev, false);

	if (client->irq) {
		free_irq(client->irq, client);
	}

	new_client = NULL;

	return 0;
}


#ifdef CONFIG_PM_SLEEP

static int lc709203f_suspend(struct device *dev)
{
	return 0;
}

static int lc709203f_resume(struct device *dev)
{
	return 0;
}

#endif /* CONFIG_PM */

static const struct i2c_device_id lc709203f_id[] = {
	{ "lc709203f", 0 },
	{ }
};
MODULE_DEVICE_TABLE( i2c, lc709203f_id);

#ifdef CONFIG_OF
static struct of_device_id lc709203f_of_id_table[] = {
	{ .compatible = "onsemi,lc709203f", },
	{}
};
MODULE_DEVICE_TABLE( of, lc709203f_of_id_table);
#endif /* CONFIG_OF */

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops lc709203f_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lc709203f_suspend, lc709203f_resume)
};
#endif

static struct i2c_driver lc709203f_i2c_driver = {
	.driver = {
		.name = "lc709203f",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lc709203f_of_id_table),
#endif /* CONFIG_OF */
#ifdef CONFIG_PM_SLEEP
		.pm   = &lc709203f_pm_ops,
#endif
	},
	.probe = lc709203f_probe,
	.remove = lc709203f_remove,
	.id_table = lc709203f_id,
};

static int __init lc709203f_init(void)
{
	return i2c_add_driver(&lc709203f_i2c_driver);
}
module_init(lc709203f_init);

static void __exit lc709203f_exit(void)
{
	i2c_del_driver(&lc709203f_i2c_driver);
}
module_exit(lc709203f_exit);

MODULE_AUTHOR("Sanyo Ltd..");
MODULE_DESCRIPTION("LC709203F");
MODULE_LICENSE("GPL");
