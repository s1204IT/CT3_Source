/*
 * Wacom Penabled Driver for I2C
 *
 * Copyright (c) 2011 Tatsunosuke Tobita, Wacom.
 * <tobita.tatsunosuke@wacom.co.jp>
 *
 * Copyright (C) 2015 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version of 2 of the License,
 * or (at your option) any later version.
 */

//#define DEBUG

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <asm/unaligned.h>

#include "wacom_i2c.h"
#include "wacom.h"

/*Below for mesuring time*/
#include <linux/sched.h>

#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif /* CONFIG_PM_RUNTIME */

#define WACOM_CMD_QUERY0	0x04
#define WACOM_CMD_QUERY1	0x00
#define WACOM_CMD_QUERY2	0x33
#define WACOM_CMD_QUERY3	0x02
#define WACOM_CMD_THROW0	0x05
#define WACOM_CMD_THROW1	0x00
//#define WACOM_QUERY_SIZE	19
#define WACOM_QUERY_SIZE	21
#define WACOM_RETRY_CNT		100
#define WACOM_PEN_DATA_SIZE	17

#define INTERRUPT_INTERVAL_TIME	150000000

#define HID_DESC_REGISTER       0x01
#define USAGE_PAGE              0x05
#define USAGE_PAGE_DIGITIZERS   0x0d
#define USAGE_PAGE_DESKTOP      0x01
#define USAGE                   0x09
#define USAGE_PEN               0x02
#define USAGE_MOUSE             0x02
#define USAGE_FINGER            0x22
#define USAGE_STYLUS            0x20
#define USAGE_TOUCHSCREEN       0x04
#define USAGE_X                 0x30
#define USAGE_TIPPRESSURE       0x30
#define USAGE_Y                 0x31

typedef struct hid_descriptors {
	u16 wHIDDescLength;
	u16 bcdVersion;
	u16 wReportDescLength;
	u16 wReportDescRegister;
	u16 wInputRegister;
	u16 wMaxInputLength;
	u16 wOutputRegister;
	u16 wMaxOutputLength;
	u16 wCommandRegister;
	u16 wDataRegister;
	u16 wVendorID;
	u16 wProductID;
	u16 wVersion;
	u16 RESERVED_HIGH;
	u16 RESERVED_LOW;
} HID_DESC;

struct wacom_features {
	HID_DESC hid_desc;
	unsigned int input_size;
	int x_max;
	int y_max;
	int x_touch;
	int y_touch;
	int pressure_max;
	int fw_version;
	int vendorId;
	int productId;
};

struct wacom_i2c {
	struct i2c_client *client;
	struct input_dev *input;
	struct workqueue_struct *wacom_wq;
	struct work_struct workq;
	struct hrtimer timer;
#ifdef WACOM_SYSFS
	struct wacom *wacom;
#endif
	u8 data[WACOM_QUERY_SIZE];
	bool prox;
	int tool;
	unsigned int x_max;
	unsigned int y_max;
	unsigned int pressure_max;
	int intr_gpio;
	bool int_enable;
};

struct timeval tv;

#ifdef WACOM_SYSFS
#define CLASS_NAME "wacom_class"
#define DEVICE_NAME "wacom_emr"

static bool bCalibrationSet;
static int  fw_version;
static NODE_STATE gNodeState = STATE_NORMAL;
static struct sPoint gSensorDims;
static struct calibrationData gCalibData = {0, 0, 20000, 10000};

static atomic_t irq_active;

/*Below wacom_read, wacom_write, and following attributes added for sysfs*/
static ssize_t wacom_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	switch(gNodeState){
	case STATE_QUERY:
		printk("%s STATE_QUERY: x_max:%d y_max:%d size:%lu\n", __func__, 
		       gSensorDims.x, gSensorDims.y, sizeof(struct sPoint));
		return sprintf(buf, "%d|%d", gSensorDims.x, gSensorDims.y);

	case STATE_NORMAL:
		printk("%s STATE_NORMAL \n", __func__);
		break;

	default:
		printk("No mode is set\n");
		break;
	}

	return sprintf(buf, "%d", 0);
}

static ssize_t wacom_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if (count == 1) {
	  switch(buf[0]){
	  case '*':
	    gNodeState = STATE_QUERY;
	    printk("%s set STATE_QUERY \n", __func__);
	    break;

	  case 0:
	  default:
	    printk("%s set STATE_NORMAL \n", __func__);
	    gNodeState = STATE_NORMAL;
	    break;
	  }

	}
	else if (count == 16) {
	  struct calibrationData *tmp = (struct calibrationData*)buf;

	  gCalibData.originX = tmp->originX;
	  gCalibData.originY = tmp->originY;
	  gCalibData.extentX = tmp->extentX;
	  gCalibData.extentY = tmp->extentY;
	 
	  bCalibrationSet = true;
	  printk("-------------------------------------------------------\n");
	  printk("%s Calibration Data Set: %d, %d, %d, %d\n", __func__, 
		 gCalibData.originX, gCalibData.originY, gCalibData.extentX, gCalibData.extentY);
	  printk("-------------------------------------------------------\n");
	}

	return count;
}

/*Below for exposing information*/
/*Format: items separated by '|'*/
static ssize_t wacom_information(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d|%d|%x", gSensorDims.x, gSensorDims.y, fw_version);
}

static ssize_t wacom_calData(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d|%d|%d|%d", gCalibData.originX, gCalibData.originY, gCalibData.extentX, gCalibData.extentY);
}

static DEVICE_ATTR(calibration, 0644, wacom_read, wacom_write);
static DEVICE_ATTR(information, 0644, wacom_information, NULL);
static DEVICE_ATTR(calData, 0644, wacom_calData, NULL);

static struct attribute *wacom_attributes[] = {
	&dev_attr_calibration.attr,
	&dev_attr_information.attr,
	&dev_attr_calData.attr,
	NULL,
};

static struct attribute_group wacom_attr_group = {
	.attrs = wacom_attributes,
};

static int register_sysfs(struct wacom *wacom)
{
	int error;
	
	/*Below added for sysfs*/
	/*If everything done without any failure, register sysfs*/
	wacom->dev_t = MKDEV(66, 0);
	wacom->class = class_create(THIS_MODULE, CLASS_NAME);
	wacom->dev = device_create(wacom->class, NULL, wacom->dev_t, NULL, DEVICE_NAME);
	if (IS_ERR(&wacom->dev)) {
		dev_err(wacom->dev,
			"Failed to create device, \"wac_i2c\"\n");
		goto err_create_class;
	}
	
	dev_set_drvdata(wacom->dev, wacom);
	
	error = sysfs_create_group(&wacom->dev->kobj, &wacom_attr_group);
	if (error) {
		dev_err(wacom->dev,
			"Failed to create sysfs group, \"wacom_attr_group\": (%d) \n", error);
		goto err_create_sysfs;
	}
	
	return 0;
	
 err_create_sysfs:
	sysfs_remove_group(&wacom->dev->kobj, &wacom_attr_group);
 err_create_class:
	device_destroy(wacom->class, wacom->dev_t);
	class_destroy(wacom->class);
	
	return -ERR_REGISTER_SYSFS;
}

static void remove_sysfs(struct wacom *wacom)
{
	/*Clear sysfs resources*/
	sysfs_remove_group(&wacom->dev->kobj, &wacom_attr_group);
	device_destroy(wacom->class, wacom->dev_t);
	class_destroy(wacom->class);
}

static void set_calib(int *x, int *y, int x_max, int y_max)
{
	int temp_coord;
	/*X-Coordination with calibration value*/
	temp_coord = *x * gCalibData.extentX / x_max + gCalibData.originX;

	/*Check if obtained coordinations don't exceeds thier maximum or get negative numbers*/
	if (temp_coord < 0)
		*x = 0;
	else if (temp_coord > x_max)
		*x = x_max;
	else
		*x = temp_coord;
	
	/*Y-Coordination with calibration value*/
	temp_coord = *y * gCalibData.extentY / y_max + gCalibData.originY;
	
	/*Check if obtained coordinations don't exceeds thier maximum or get negative numbers*/
	if (temp_coord < 0)
		*y = 0;
	else if (temp_coord > y_max)
		*y = y_max;
	else
		*y = temp_coord;
}
#endif // WACOM_SYSFS

static void parse_report_desc(struct wacom_features *features, u8 *report_desc, int report_desc_size)
{
	bool finger = false, pen = false;
	int i;
	int usage = 0, mouse = 0;

	for (i = 0; i < report_desc_size; i++) {
		switch (report_desc[i]) {
		case USAGE_PAGE:
			switch (report_desc[i + 1]) {
			case USAGE_PAGE_DIGITIZERS:
				usage = USAGE_PAGE_DIGITIZERS;
				if (report_desc[i + 3] == USAGE_TOUCHSCREEN || report_desc[i + 3] == USAGE_PEN) {
					mouse = 0;
					i += 4;
				}
				else
					i++;
				
				break;

			case USAGE_PAGE_DESKTOP:
				usage = USAGE_PAGE_DESKTOP;
				if (report_desc[i + 3] == USAGE_MOUSE) {
					mouse = 1;
					i += 4;
				}
				else
					i++;
				
				break;
			}
			break;

		case USAGE:
			switch (report_desc[i + 1]) {
			case USAGE_X:
				if (usage == USAGE_PAGE_DESKTOP && mouse == 0) {
					if (pen)
					{
						
						features->x_max = get_unaligned_le16(&report_desc[i + 3]);
					}
					else if (finger)
						features->x_touch = get_unaligned_le16(&report_desc[i + 3]);
					i += 4;
				}
				else if (usage == USAGE_PAGE_DIGITIZERS && mouse == 0) {
					if (pen)
						features->pressure_max = get_unaligned_le16(&report_desc[i + 3]);
					i += 4;
				}
				else
					i++;
				
				break;
				
			case USAGE_Y:
				if (usage == USAGE_PAGE_DESKTOP && mouse == 0) {
					if (pen)
						features->y_max = get_unaligned_le16(&report_desc[i + 3]);
					else if (finger)
						features->y_touch = get_unaligned_le16(&report_desc[i + 3]);
					i += 4;
				}
				else
					i++;
				
				break;
			case USAGE_FINGER:
				finger = true;
				pen = false;
				i++;
				break;

			case USAGE_STYLUS:
				pen = true;
				finger = false;
				i++;
				break;
			}
			break;
		}		
	}
}

static int wacom_get_report_descriptor(struct i2c_client *client, struct wacom_features *features,
			 HID_DESC hid_desc)
{
#ifdef SHOW_REPORTDESC
	int i = 0;
#endif
	int ret = -1;
	int report_desc_size = hid_desc.wReportDescLength;
	u8 cmd_reportDesc[] = {hid_desc.wReportDescRegister, 0x00};
	u8 *report_desc = NULL;
	struct i2c_msg msgs_desc[2];

	dev_dbg(&client->dev, "Retrieving report descriptor\n");
	report_desc = kzalloc(sizeof(u8) * report_desc_size, GFP_KERNEL);
	if (!report_desc) {
		dev_err(&client->dev, "No memory left for this device\n");
		return -ENOMEM;
	}

	msgs_desc[0].addr = client->addr;
	msgs_desc[0].flags = 0;
	msgs_desc[0].len = sizeof(cmd_reportDesc);
	msgs_desc[0].buf = cmd_reportDesc;

	msgs_desc[1].addr = client->addr;
	msgs_desc[1].flags = I2C_M_RD;
	msgs_desc[1].len = report_desc_size;
	msgs_desc[1].buf = report_desc;
		
	ret = i2c_transfer(client->adapter, msgs_desc, ARRAY_SIZE(msgs_desc));
	if (ret < 0) {
		dev_err(&client->dev, "%s obtaining report descriptor failed\n", __func__);
		goto errReportDesc;
	}
	if (ret != ARRAY_SIZE(msgs_desc)) {
		ret = -EIO;
		goto errReportDesc;
	}

#ifdef SHOW_REPORTDESC
	for (i = 0; i < report_desc_size; i ++) {
		if( i%8 == 0)
			printk( "\n RP%d:0x ", i );
		printk("%02x ",  report_desc[i]);
	}
#endif

	parse_report_desc(features, report_desc, report_desc_size);
	ret = 0;
	dev_dbg(&client->dev, "addr: %x x_max:%d, y_max:%d\n", client->addr, 
	       features->x_max, features->y_max);
	dev_dbg(&client->dev, "addr: %x x_touch:%d, y_touch:%d\n", client->addr, 
	       features->x_touch, features->y_touch);
	dev_dbg(&client->dev, "pressure_max:%d, fw_version:%x\n",
	       features->pressure_max, features->fw_version);
	
errReportDesc:
	kfree(report_desc);
	report_desc = NULL;
	return ret;
}

int wacom_query_device(struct i2c_client *client, struct wacom_features *features)
{
	int ret = -1;
	u8 cmd_devDesc[] = {HID_DESC_REGISTER, 0x00};
	HID_DESC hid_descriptor = {0};		
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd_devDesc),
			.buf = cmd_devDesc,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(HID_DESC),
			.buf = (u8 *)(&hid_descriptor),
		},
	};
	
	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		dev_err(&client->dev, "%s obtaining query failed: %d\n", __func__, ret);
		goto errQueryDevice;
	}
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "%s input/output error occured;\n returned: %dbyte(s)\n", __func__, ret);
		ret = -EIO;
		goto errQueryDevice;
	}

	features->input_size = hid_descriptor.wMaxInputLength;
	features->vendorId = hid_descriptor.wVendorID;
	features->productId = hid_descriptor.wProductID;
	features->fw_version = hid_descriptor.wVersion;
	memcpy(&features->hid_desc, &hid_descriptor, sizeof(HID_DESC));	

	printk(
		"dev_VID:0x%x, dev_PID:0x%x dev_Version:0x%x\n",
		hid_descriptor.wVendorID, hid_descriptor.wProductID, hid_descriptor.wVersion );

	ret = wacom_get_report_descriptor(client, features, hid_descriptor);
	if (ret < 0)
		goto errQueryDevice;

	ret = 0;
errQueryDevice:
	return ret;
}

static void wacom_workqueue(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c = container_of(work, struct wacom_i2c, workq);
		struct input_dev *input = wac_i2c->input;
	u8 *data = wac_i2c->data;
	unsigned int x, y, pressure;
	unsigned char tsw, f1;
	int error;

	struct i2c_client *client = wac_i2c->client;
	struct wacom_i2c_platform_data *pdata = wac_i2c->client->dev.platform_data;

	if (gpio_get_value(wac_i2c->intr_gpio))
	{
//		printk("[wacom] Detected the jitter on INT pin\n");
		return;
	}
	
	/*
	error = i2c_master_recv(wac_i2c->client,
				wac_i2c->data, sizeof(wac_i2c->data));
	if (error < 0)
		goto out;
	*/
	while(gpio_get_value(wac_i2c->intr_gpio) == 0) {
		memset( data, 0, WACOM_PEN_DATA_SIZE );
		error = i2c_master_recv( client, data, WACOM_PEN_DATA_SIZE );
		if ( error < 0 ) {
			dev_err( &client->dev,
					 "i2c_master_recv(&data[0], 2) failed. (%d)\n", error );
			continue;
		}

		if ( le16_to_cpup( (__le16 *)data ) != WACOM_PEN_DATA_SIZE ) {
			dev_err( &client->dev,
					 "%s: unexpected data length %d\n",
					 __FUNCTION__, le16_to_cpup( (__le16 *)data ) );
			continue;
		}
		if ( data[2] != 0x02 ) {
			dev_err( &client->dev,
					 "%s: unexpected report id %d\n",
					 __FUNCTION__, data[2] );
			continue;
		}

		tsw = data[3] & 0x01;
		f1 = data[3] & 0x02;
		x = le16_to_cpup((__le16 *)&data[4]);
		y = le16_to_cpup((__le16 *)&data[6]);
printk("!!!!!!!%d,%d\n",x,y);
		pressure = le16_to_cpup((__le16 *)&data[8]);
#if 0
		if ( (x > wac_i2c->x_max) ||
			 (y > wac_i2c->y_max) ||
			 (pressure > wac_i2c->pressure_max) ) {
			dev_dbg( &client->dev,
					  "Invalid data received.\n" );
			dev_dbg( &client->dev,
					  "READ DATA:"
					  " %02x %02x %02x %02x %02x %02x %02x\n",
					  data[3], data[4], data[5], data[6],
					  data[7], data[8], data[9] );
			continue;
		}
#endif
		if (pdata->invert_x) {
			x = wac_i2c->x_max - x;
		}
		if (pdata->invert_y) {
			y = wac_i2c->y_max - y;
		}
		if (pdata->swap_xy) {
			int tmp = x;
			x = y;
			y = tmp;
		}
		dev_dbg( &client->dev,
			     "x:%5d, y:%5d, p:%5d, f:%02x\n", x, y, pressure, data[3] );

//		do_gettimeofday(&tv);

#ifdef WACOM_SYSFS
		/*Set calibration values*/
		if (bCalibrationSet && gNodeState == STATE_NORMAL)
			set_calib(&x, &y, wac_i2c->x_max, wac_i2c->y_max);
#endif

		wac_i2c->prox = data[3] & 0x20;

		input_report_key(input, BTN_TOUCH, tsw);
		input_report_key(input, BTN_TOOL_PEN, wac_i2c->prox && (!f1));
		input_report_key(input, BTN_TOOL_RUBBER, f1);
		input_report_abs(input, ABS_X, x);
		input_report_abs(input, ABS_Y, y);
		input_report_abs(input, ABS_PRESSURE, pressure);
		input_sync(input);
	}
}

static irqreturn_t wacom_i2c_irq(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	int ret;

//	disable_irq_nosync(wac_i2c->client->irq);

	atomic_set(&irq_active, 1);
	hrtimer_start(&wac_i2c->timer, ktime_set(0, INTERRUPT_INTERVAL_TIME), HRTIMER_MODE_REL);

	ret = queue_work(wac_i2c->wacom_wq, &wac_i2c->workq);
	if(ret != 1)
	{
//		dev_err(&wac_i2c->client->dev, "wacom_i2c_irq queue_work %d\n",ret);
	}

	return IRQ_HANDLED;
}

static enum hrtimer_restart wacom_hrtimer_func(struct hrtimer *timer)
{
	atomic_set(&irq_active, 0);
	return HRTIMER_NORESTART;
}

int wacom_i2c_is_active(void)
{
	if (atomic_read(&irq_active))
		return 1;
	return 0;
}

static int wacom_i2c_open(struct input_dev *dev)
{
//	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
//	struct i2c_client *client = wac_i2c->client;

//	enable_irq(client->irq);

	return 0;
}

static void wacom_i2c_close(struct input_dev *dev)
{
//	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
//	struct i2c_client *client = wac_i2c->client;

//	disable_irq(client->irq);
}

#ifdef CONFIG_OF
static int wacom_i2c_dt_get_gpio(struct i2c_client *client, const char *name)
{
    int ret;

    ret = of_get_named_gpio(client->dev.of_node, name, 0);
    if (ret < 0) {
		dev_err(&client->dev,
			"of_get_named_gpio(\"%s\") failed. (%d)\n", name, ret);
		return -1;
    }
    return ret;
}

static int wacom_i2c_probe_dt(struct i2c_client *client)
{
	struct device_node 		*np	= client->dev.of_node;
	struct wacom_i2c_platform_data	*pdata	= client->dev.platform_data;

	u32 tmp;

	pdata->irq_gpio   = wacom_i2c_dt_get_gpio(client, "int-gpio");
	pdata->reset_gpio = wacom_i2c_dt_get_gpio(client, "rst-gpio");
	pdata->power_gpio = wacom_i2c_dt_get_gpio(client, "power-gpio");

	if (gpio_is_valid(pdata->power_gpio)) {
		dev_dbg(&client->dev, "power_gpio=%d", pdata->power_gpio);
		gpio_request(pdata->power_gpio, NULL);
		gpio_direction_output(pdata->power_gpio, 1);
		msleep(100);
		gpio_free(pdata->power_gpio);
	}
	else {	
		dev_err(&client->dev,
			"power_gpio(%d) is invalid.\n", pdata->power_gpio);
		return -1;
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		dev_dbg(&client->dev, "irq_gpio=%d", pdata->irq_gpio);
		gpio_request(pdata->irq_gpio, NULL);
		gpio_direction_input(pdata->irq_gpio);
		gpio_free(pdata->irq_gpio);
		dev_dbg(&client->dev, "client->irq=%d->%d",
			client->irq, gpio_to_irq(pdata->irq_gpio));
		client->irq = gpio_to_irq(pdata->irq_gpio);
	}
	else {
		dev_err(&client->dev,
			"irq_gpio(%d) is invalid.\n", pdata->irq_gpio);
		return -1;
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		dev_dbg(&client->dev, "reset_gpio=%d", pdata->reset_gpio);
		gpio_request(pdata->reset_gpio, NULL);
		gpio_direction_output(pdata->reset_gpio, 1);
		gpio_free(pdata->reset_gpio);
	}
	else {	
		dev_err(&client->dev,
			"reset_gpio(%d) is invalid.\n", pdata->reset_gpio);
		return -1;
	}

	if (0 == of_property_read_u32(np, "wacom,swap_xy", &tmp)) {
	    pdata->swap_xy = tmp;
	}
	else {
	    pdata->swap_xy = 0;
	}

	if (0 == of_property_read_u32(np, "wacom,invert_x", &tmp)) {
	    pdata->invert_x = tmp;
	}
	else {
	    pdata->invert_x = 0;
	}

	if (0 == of_property_read_u32(np, "wacom,invert_y", &tmp)) {
	    pdata->invert_y = tmp;
	}
	else {
	    pdata->invert_y = 0;
	}

	return 0;
}
#endif /* CONFIG_OF */

static void wacom_i2c_reset(struct i2c_client *client)
{
	struct wacom_i2c_platform_data *pdata = client->dev.platform_data;

	if (pdata != NULL) {
		if (gpio_is_valid(pdata->reset_gpio)) {
			gpio_request(pdata->reset_gpio, "wacom_reset");
			gpio_direction_output(pdata->reset_gpio, 0);
			msleep(100);
			gpio_direction_output(pdata->reset_gpio, 1);
			msleep(100);
			gpio_free(pdata->reset_gpio);
		}
	}
}

static int wacom_i2c_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct wacom_i2c *wac_i2c;
	struct input_dev *input;
	struct wacom_features features;
	struct wacom_i2c_platform_data *pdata;

	int error;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}

#ifdef CONFIG_OF
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		dev_err(&client->dev, "kzalloc(platform_data) failed.\n");
		return -ENOMEM;
	}
	client->dev.platform_data = pdata;
	error = wacom_i2c_probe_dt(client);
	if (error != 0) {
		dev_err(&client->dev,
			"wacom_i2c_probe_dt() failed.(%d)\n", error);
		kfree(pdata);
		return -EINVAL;
	}
	pdata = client->dev.platform_data;
#else  /* !CONFIG_OF */
	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&client->dev, "No platform data found.\n");
		return -EINVAL;
	}
#endif /* !CONFIG_OF */

	wacom_i2c_reset(client);

	atomic_set(&irq_active, 0);

	error = wacom_query_device(client, &features);
	if (error)
		return error;

	wac_i2c = kzalloc(sizeof(*wac_i2c), GFP_KERNEL);

	input = input_allocate_device();
	if (!wac_i2c || !input) {
		error = -ENOMEM;
		goto err_free_mem;
	}

	wac_i2c->client = client;
	wac_i2c->input = input;
	wac_i2c->x_max = features.x_max;
	wac_i2c->y_max = features.y_max;
	wac_i2c->pressure_max = features.pressure_max;
	wac_i2c->intr_gpio = pdata->irq_gpio;

	input->name = "Wacom I2C Digitizer";
	input->id.bustype = BUS_I2C;
	input->id.vendor = 0x56a;
	input->id.version = features.fw_version;
	input->dev.parent = &client->dev;
	input->open = wacom_i2c_open;
	input->close = wacom_i2c_close;

	input->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	__set_bit(BTN_TOOL_PEN, input->keybit);
	__set_bit(BTN_TOOL_RUBBER, input->keybit);
	__set_bit(BTN_STYLUS, input->keybit);
	__set_bit(BTN_STYLUS2, input->keybit);
	__set_bit(BTN_TOUCH, input->keybit);

	if(pdata->swap_xy == 0)
	{
		input_set_abs_params(input, ABS_X, 0, features.x_max, 0, 0);
		input_set_abs_params(input, ABS_Y, 0, features.y_max, 0, 0);
	}
	else
	{
		input_set_abs_params(input, ABS_X, 0, features.y_max, 0, 0);
		input_set_abs_params(input, ABS_Y, 0, features.x_max, 0, 0);
	}

	input_set_abs_params(input, ABS_PRESSURE,
			     0, features.pressure_max, 0, 0);

#ifdef WACOM_SYSFS
	/*Below added for Calibration purpose*/
	/*Initializing calibraion values*/
	gCalibData.originX = gCalibData.originY = 0;
	gCalibData.extentX = gSensorDims.x = features.x_max;
	gCalibData.extentY = gSensorDims.y = features.y_max;
	fw_version = features.fw_version;
#endif

	input_set_drvdata(input, wac_i2c);

	hrtimer_init(&wac_i2c->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	wac_i2c->timer.function = wacom_hrtimer_func;

	error = input_register_device(wac_i2c->input);
	if (error) {
		dev_err(&client->dev,
			"Failed to register input device, error: %d\n", error);
		goto err_free_mem;
	}

	i2c_set_clientdata(client, wac_i2c);

	wac_i2c->wacom_wq = create_singlethread_workqueue("wacom_wq");
	if (!wac_i2c->wacom_wq) {
		printk(KERN_ERR "[wacom] %s: create workqueue failed\n", __func__);
		error = -ENOMEM;
		goto err_create_wq_failed;
	}
	INIT_WORK(&wac_i2c->workq, wacom_workqueue);

	error = request_threaded_irq(client->irq, NULL, wacom_i2c_irq,
				     IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				     "wacom_i2c", wac_i2c);
	if (error) {
		dev_err(&client->dev,
			"Failed to enable IRQ, error: %d\n", error);
		goto err_free_mem;
	}
	wac_i2c->int_enable = true;

	if (gpio_get_value(wac_i2c->intr_gpio) == 0) {
		printk(KERN_INFO "[wacom]%s: handle missed interrupt\n", __func__);
		wacom_i2c_irq(client->irq, wac_i2c);
	}
	
#ifdef CONFIG_PM_RUNTIME
//	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
#endif /* CONFIG_PM_RUNTIME */

	/* Disable the IRQ, we'll enable it in wac_i2c_open() */
//	disable_irq(client->irq);

#ifdef WACOM_SYSFS
	wac_i2c->wacom = kzalloc(sizeof(struct wacom), GFP_KERNEL);
	if (!wac_i2c->wacom) {
		error = -ENOMEM;
		goto err_free_irq;
	}
	error = register_sysfs(wac_i2c->wacom);
	if (error == -ERR_REGISTER_SYSFS) {
		goto err_remove_sysfs;
	}

	return 0;

err_remove_sysfs:
	remove_sysfs(wac_i2c->wacom);
	kfree(wac_i2c->wacom);
#endif

err_free_irq:
	free_irq(client->irq, wac_i2c);
err_create_wq_failed:
	if (wac_i2c->wacom_wq)
		destroy_workqueue(wac_i2c->wacom_wq);
err_free_mem:
	input_free_device(input);
	kfree(wac_i2c);

	return error;
}

static int wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

#ifdef WACOM_SYSFS
	remove_sysfs(wac_i2c->wacom);
#endif
	free_irq(client->irq, wac_i2c);
	if (wac_i2c->wacom_wq)
		destroy_workqueue(wac_i2c->wacom_wq);
	input_unregister_device(wac_i2c->input);
	kfree(wac_i2c);

	return 0;
}

static int wacom_i2c_suspend(struct device *dev)
{
	return 0;
}

static int wacom_i2c_resume(struct device *dev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int wacom_i2c_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	int ret;
	u8 cmd[] = { 0x04, 0x00, 0x01, 0x08 };
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = cmd,
		},
	};

	if(wac_i2c->int_enable == true)
	{
		disable_irq(client->irq);
		wac_i2c->int_enable = false;
	}
	flush_workqueue(wac_i2c->wacom_wq);

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		printk("[wacom] suspend failed\n");

	return 0;
}

static int wacom_i2c_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	int ret;
	u8 cmd[] = { 0x04, 0x00, 0x00, 0x08 };
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = cmd,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		printk("[wacom] resume failed\n");

	if(wac_i2c->int_enable != true)
	{
		enable_irq(client->irq);
		wac_i2c->int_enable = true;
	}

	if (gpio_get_value(wac_i2c->intr_gpio) == 0)
	{
		printk("[wacom] Detected the jitter on INT pin");
		ret = queue_work(wac_i2c->wacom_wq, &wac_i2c->workq);
		if(ret != 1)
			printk("wacom_i2c_irq queue_work %d\n",ret);
	}

	return 0;
}
#endif /* CONFIG_PM_RUNTIME */

#ifdef CONFIG_PM_SLEEP
static struct dev_pm_ops wacom_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		wacom_i2c_suspend,
		wacom_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		wacom_i2c_runtime_suspend,
		wacom_i2c_runtime_resume,
		NULL)
#endif /* CONFIG_PM_RUNTIME */
};
#endif /* CONFIG_PM_SLEEP */

static const struct i2c_device_id wacom_i2c_id[] = {
	{ "WAC_I2C_EMR", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, wacom_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id wacom_i2c_of_idtable[] = {
	{ .compatible = "wacom,i2c", },
	{}
};
#endif /* CONFIG_OF */

static struct i2c_driver wacom_i2c_driver = {
	.probe		= wacom_i2c_probe,
	.remove		= wacom_i2c_remove,
	.id_table	= wacom_i2c_id,
	.driver	= {
		.name	= "wacom_i2c",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm	= &wacom_i2c_pm_ops,
#endif /* CONFIG_PM_SLEEP */
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(wacom_i2c_of_idtable),
#endif /* CONFIG_OF */
	},
};

static int __init wacom_i2c_init(void)
{
	return i2c_add_driver(&wacom_i2c_driver);
}

static void __exit wacom_i2c_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}

module_init(wacom_i2c_init);
module_exit(wacom_i2c_exit);

MODULE_AUTHOR("Tatsunosuke Tobita <tobita.tatsunosuke@wacom.co.jp>");
MODULE_DESCRIPTION("WACOM EMR I2C Driver");
MODULE_LICENSE("GPL");
