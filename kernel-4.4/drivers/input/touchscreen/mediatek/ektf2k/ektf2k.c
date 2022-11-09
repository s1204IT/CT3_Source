/* drivers/input/touchscreen/ektf2k.c - ELAN EKTF2K verions of driver
 *
 * Copyright (C) 2011 Elan Microelectronics Corporation.
 * Copyright (C) 2017 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 2011/12/06: The first release, version 0x0001
 * 2012/2/15:  The second release, version 0x0002 for new bootcode
 * 2012/5/8:   Release version 0x0003 for china market
 *             Integrated 2 and 5 fingers driver code together and
 *             auto-mapping resolution.
 * 2012/12/1:  Release version 0x0005: support up to 10 fingers but no buffer mode.
 *             Please change following parameters
 *                 1. For 5 fingers protocol, please enable ELAN_PROTOCOL.
                      The packet size is 18 or 24 bytes.
 *                 2. For 10 fingers, please enable both ELAN_PROTOCOL and ELAN_TEN_FINGERS.
                      The packet size is 40 or 4+40+40+40 (Buffer mode) bytes.
 *                 3. Please enable the ELAN_BUTTON configuraton to support button.
                   4. For ektf3k serial, Add Re-Calibration Machanism 
 *                    So, please enable the define of RE_CALIBRATION.
 *                   
 *
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/kthread.h>

// for linux 2.6.36.3
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/ioctl.h>
#include <linux/switch.h>
#include <linux/proc_fs.h>
#include <linux/wakelock.h>

//#define ELAN_TEN_FINGERS

#include "ektf2k.h"
#include "tpd.h"

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#endif /* CONFIG_OF */

static DEFINE_MUTEX(i2c_access);
static int tpd_halt = 0;
static int tpd_flag = 0;
unsigned int touch_irq = 0;
static DECLARE_WAIT_QUEUE_HEAD(waiter);

#ifdef ELAN_TEN_FINGERS
#define PACKET_SIZE		45		/* support 10 fingers packet for nexus7 55 */
#else
#define PACKET_SIZE		24			/* support 5 fingers packet  */
#endif

#define CMD_S_PKT		0x52
#define CMD_R_PKT		0x53
#define CMD_W_PKT		0x54

#define HELLO_PKT		0x55

#define TWO_FINGERS_PKT		0x5A
#define FIVE_FINGERS_PKT	0x5D
#define MTK_FINGERS_PKT		0x6D
#define TEN_FINGERS_PKT		0x62
#define BUFFER_PKT			0x63
#define BUFFER55_PKT		0x66

#define RESET_PKT		0x77
#define CALIB_PKT		0x66


uint8_t RECOVERY=0x00;
int FW_VERSION=0x00;

int X_RESOLUTION=2368;
int Y_RESOLUTION=1472;

int FW_ID=0x00;

struct elan_ktf2k_ts_data {
	struct i2c_client *client;
	int intr_gpio;
	int rst_gpio;
	int fw_ver;
	int fw_id;
	int x_resolution;
	int y_resolution;
	int disable;
	bool int_enable;
	bool suspend;
	int orient;
};

static struct elan_ktf2k_ts_data *private_ts;
static int __fw_packet_handler(struct i2c_client *client);
static void elan_ktf2k_ts_report_data(uint8_t *buf);

/************************************
* Restet TP 
*************************************/
void elan_ktf2k_ts_hw_reset(struct i2c_client *client)
{
	int ret;
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);

	printk("[ELAN] Start HW reset!\n");
	ret = gpio_request(ts->rst_gpio, "tp_rst_n");
	if (ret) {
		printk("ERROR: failed to request reset gpio !!!\n");
		return;
	}
	gpio_direction_output(ts->rst_gpio, 0);
	msleep(20);
	gpio_direction_output(ts->rst_gpio, 1);
	msleep(10);
	gpio_free(ts->rst_gpio);
}

static int __elan_ktf2k_ts_poll(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int status = 0, retry = 10;

	do {
		status = gpio_get_value(ts->intr_gpio);
		printk("%s: status = %d\n", __func__, status);
		retry--;
		mdelay(50);
	} while (status == 1 && retry > 0);

//	printk( "[elan]%s: poll interrupt status %s\n",
//			__func__, status == 1 ? "high" : "low");
	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_ktf2k_ts_poll(struct i2c_client *client)
{
	return __elan_ktf2k_ts_poll(client);
}

static uint8_t buf_recv[4] = { 0 };

static int __hello_packet_handler(struct i2c_client *client)
{
	int rc;
//	uint8_t buf_recv[8] = { 0 };

	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0) {
		printk( "[elan] %s: Int poll failed!\n", __func__);
		RECOVERY=0x80;
		return RECOVERY;
		//return -EINVAL;
	}

	rc = i2c_master_recv(client, buf_recv, 4);
	printk("[elan] %s: hello packet %2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);

	if(!(buf_recv[0]==0x55 && buf_recv[1]==0x55 && buf_recv[2]==0x55 && buf_recv[3]==0x55))
	{
		RECOVERY=0x80;
		return RECOVERY; 
	}
	mdelay(200);

	return 0;
}

static struct i2c_msg msg[1];

static int elan_ktf3k_ts_read_command(struct i2c_client *client,
			   u8* cmd, u16 cmd_length, u8 *value, u16 value_length){
	struct i2c_adapter *adapter = client->adapter;
//	struct i2c_msg msg[1];
	struct elan_ktf2k_ts_data *ts;
	int length = 0;

	ts = i2c_get_clientdata(client);

	msg[0].addr = client->addr;
	msg[0].flags = 0x00;
	msg[0].len = cmd_length;
	msg[0].buf = cmd;

	length = i2c_transfer(adapter, msg, 1);
	
	if (length == 1) // only send on packet
		return value_length;
	else
		return -EIO;
}

static int elan_ktf3k_i2c_read_packet(struct i2c_client *client, 
	u8 *value, u16 value_length){
	struct i2c_adapter *adapter = client->adapter;
//	struct i2c_msg msg[1];
	struct elan_ktf2k_ts_data *ts;
	int length = 0;

	ts = i2c_get_clientdata(client);

	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = value_length;
	msg[0].buf = (u8 *) value;
	length = i2c_transfer(adapter, msg, 1);
	
	if (length == 1) // only send on packet
		return value_length;
	else
		return -EIO;
}

static int __fw_packet_handler(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int rc;
	int major, minor;
	int immediate = 1;

	static uint8_t cmd[] = {CMD_R_PKT, 0x00, 0x00, 0x01};
	static uint8_t cmd_x[] = {0x53, 0x60, 0x00, 0x00}; /*Get x resolution*/
	static uint8_t cmd_y[] = {0x53, 0x63, 0x00, 0x00}; /*Get y resolution*/
	static uint8_t cmd_id[] = {0x53, 0xf0, 0x00, 0x01}; /*Get firmware ID*/
//	uint8_t buf_recv[4] = {0};
//	Firmware version
	rc = elan_ktf3k_ts_read_command(client, cmd, 4, buf_recv, 4);
	if (rc < 0)
		return rc;
	
	if(immediate){
		rc = elan_ktf2k_ts_poll(client);
		elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
		major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
		minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
		ts->fw_ver = major << 8 | minor;
		FW_VERSION = ts->fw_ver;
//		touch_debug(DEBUG_INFO, "[elan] %s: firmware version: 0x%4.4x\n", __func__, ts->fw_ver);
		printk("[elan] %s: firmware version: 0x%4.4x\n", __func__, ts->fw_ver);
	}
// X Resolution
	rc = elan_ktf3k_ts_read_command(client, cmd_x, 4, buf_recv, 4);
	if (rc < 0)
		return rc;

	if(immediate){
		rc = elan_ktf2k_ts_poll(client);
		elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
		minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
		ts->x_resolution =minor;
		if(ts->orient == 0) {
			Y_RESOLUTION = ts->x_resolution;
			printk("[elan] %s: Y resolution: %d\n", __func__, ts->x_resolution);
		}
		else {
			X_RESOLUTION = ts->x_resolution;
			printk("[elan] %s: X resolution: %d\n", __func__, ts->x_resolution);
		}
	}
// Y Resolution	
	rc = elan_ktf3k_ts_read_command(client, cmd_y, 4, buf_recv, 4);
	if (rc < 0)
		return rc;

	if(immediate){
		rc = elan_ktf2k_ts_poll(client);
		elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
		minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
		ts->y_resolution =minor;
		if(ts->orient == 0) {
			X_RESOLUTION = ts->y_resolution;
			printk("[elan] %s: X resolution: %d\n", __func__, ts->y_resolution);
		}
		else {
			Y_RESOLUTION = ts->y_resolution;
			printk("[elan] %s: Y resolution: %d\n", __func__, ts->y_resolution);
		}
	}
// Firmware ID
	rc = elan_ktf3k_ts_read_command(client, cmd_id, 4, buf_recv, 4);
	if (rc < 0)
		return rc;

	if(immediate){
		rc = elan_ktf2k_ts_poll(client);
		elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
		major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
		minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
		ts->fw_id = major << 8 | minor;
		FW_ID = ts->fw_id;
		printk("[elan] %s: firmware id: 0x%4.4x\n", __func__, ts->fw_id);
	}
	return 0;
}

static inline int elan_ktf2k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y)
{
	*x = *y = 0;

	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];

	return 0;
}

static int elan_ktf2k_ts_setup(struct i2c_client *client)
{
	int rc;

	rc = __hello_packet_handler(client);
	printk("[elan] hellopacket's rc = %d\n",rc);

	if (rc != 0x80){
		rc = __fw_packet_handler(client);
		if (rc < 0)
			printk("[elan] %s, fw_packet_handler fail, rc = %d", __func__, rc);
		dev_dbg(&client->dev, "[elan] %s: firmware checking done.\n", __func__);
		//Check for FW_VERSION, if 0x0000 means FW update fail!
		if ( FW_VERSION == 0x00) {
			rc = 0x80;
			printk("[elan] FW_VERSION = %d, last FW update fail\n", FW_VERSION);
		}
	}

	return rc;
}

static int elan_ktf2k_ts_recv_data(struct i2c_client *client, uint8_t *buf, int bytes_to_recv)
{
	int rc;
	if (buf == NULL)
		return -EINVAL;

	memset(buf, 0, bytes_to_recv);

	mutex_lock(&i2c_access);
/* The ELAN_PROTOCOL support normanl packet format */	
	rc = i2c_master_recv(client, buf, bytes_to_recv);
//printk("[elan] Elan protocol rc = %d \n", rc);
	mutex_unlock(&i2c_access);
	if (rc != bytes_to_recv) {
		dev_err(&client->dev, "[elan] %s: i2c_master_recv error?! \n", __func__);
		return -1;
	}

	return rc;
}

static void elan_ktf2k_ts_report_data(uint8_t *buf)
{
	uint16_t x, y;
	uint16_t fbits=0;
	uint8_t i, num, reported = 0;
	uint8_t idx;
	int finger_num;

/* for 10 fingers */
	if (buf[0] == TEN_FINGERS_PKT){
		finger_num = 10;
		num = buf[2] & 0x0f; 
		fbits = buf[2] & 0x30;	
		fbits = (fbits << 4) | buf[1]; 
		idx = 3;
	}
/* for 5 fingers */
	else if ((buf[0] == MTK_FINGERS_PKT) || (buf[0] == FIVE_FINGERS_PKT)){
		finger_num = 5;
		num = buf[1] & 0x07; 
		fbits = buf[1] >>3;
		idx = 2;
	}else{
/* for 2 fingers */
		finger_num = 2;
		num = buf[7] & 0x03;		// for elan old 5A protocol the finger ID is 0x06
		fbits = buf[7] & 0x03;
//		fbits = (buf[7] & 0x03) >> 1;	// for elan old 5A protocol the finger ID is 0x06
		idx = 1;
	}

	switch (buf[0]) {
		case MTK_FINGERS_PKT:
		case TWO_FINGERS_PKT:
		case FIVE_FINGERS_PKT:
		case TEN_FINGERS_PKT:
			if (num == 0) {
				input_report_key(tpd->dev, BTN_TOUCH, 0);
			} else {
				for (i = 0; i < finger_num; i++) {	
					if ((fbits & 0x01)) {
						if(private_ts->orient == 0)
							elan_ktf2k_ts_parse_xy(&buf[idx], &y, &x);
						else
							elan_ktf2k_ts_parse_xy(&buf[idx], &x, &y);

						if(private_ts->orient == 0)
							x = X_RESOLUTION-x;
						y = Y_RESOLUTION-y;

						printk("%s, x=%d, y=%d\n",__func__, x , y);

						if (!((x<=0) || (y<=0) || (x>=X_RESOLUTION) || (y>=Y_RESOLUTION))) {
							input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, i);
							input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
							input_report_abs(tpd->dev, ABS_MT_PRESSURE, 80);
							input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
							input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
							input_report_key(tpd->dev, BTN_TOUCH, 1);
							input_mt_sync(tpd->dev);
							reported++;
						} // end if border
					} // end if finger status
					fbits = fbits >> 1;
					idx += 3;
				} // end for
			}
			if (reported)
				input_sync(tpd->dev);
			else {
				input_mt_sync(tpd->dev);
				input_sync(tpd->dev);
			}
			break;
		default:
			printk("[elan] %s: unknown packet type: %0x\n", __func__, buf[0]);
			break;
	} // end switch

	return;
}

static uint8_t buf[4 + PACKET_SIZE] = { 0 };

static int elan_ktf2k_ts_work_func(void *unused)
{
	struct sched_param param = { .sched_priority = 4 };
	int rc;

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);

		if (gpio_get_value(private_ts->intr_gpio))
		{
//			printk("[elan] Detected the jitter on INT pin");
			continue;
		}

		disable_irq(touch_irq);

		while(gpio_get_value(private_ts->intr_gpio) == 0) {
			rc = elan_ktf2k_ts_recv_data(private_ts->client, buf, 4 + PACKET_SIZE);
			if (rc < 0)
			{
				printk("[elan] Received the packet Error.\n");
				continue;
			}

			if (private_ts->disable) {
				input_report_key(tpd->dev, BTN_TOUCH, 0);
				input_mt_sync(tpd->dev);
				input_sync(tpd->dev);
				continue;
			}

//			printk("[elan_debug] %2x,%2x,%2x,%2x,%2x,%2x,%2x,%2x ....., %2x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[17]);

			elan_ktf2k_ts_report_data(buf);
		}

		enable_irq(touch_irq);

	} while (!kthread_should_stop());

	return 0;
}

static irqreturn_t tpd_interrupt_handler(int irq, void *dev_id)
{
	tpd_flag = 1;
	wake_up_interruptible(&waiter);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF

static struct of_device_id elan_ktf2k_of_idtable[] = {
	{ .compatible = "mediatek,cap_touch", },
	{}
};

static int elan_ktf2k_ts_probe_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct elan_ktf2k_i2c_platform_data *pdata = NULL;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		dev_err(&client->dev, "kzalloc(platform_data) failed.\n");
		return -ENOMEM;
	}
	client->dev.platform_data = pdata;

	if (np) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(elan_ktf2k_of_idtable), &client->dev);
		if (!match) {
			printk(KERN_ERR "Error: No device match found\n");
			return -ENODEV;
		}
	}
	pdata->rst_gpio = of_get_named_gpio(np, "rst-gpio", 0);
	pdata->intr_gpio = of_get_named_gpio(np, "int-gpio", 0);

	if (gpio_is_valid(pdata->intr_gpio)) {
		dev_dbg(&client->dev, "irq_gpio=%d", pdata->intr_gpio);
		gpio_request(pdata->intr_gpio, NULL);
		gpio_direction_input(pdata->intr_gpio);
//		gpio_free(pdata->intr_gpio);
		dev_dbg(&client->dev, "client->irq=%d->%d",
			client->irq, gpio_to_irq(pdata->intr_gpio));
		client->irq = gpio_to_irq(pdata->intr_gpio);
	}
	else {
		dev_err(&client->dev,
			"irq_gpio(%d) is invalid.\n", pdata->intr_gpio);
		kfree(pdata);
		return -EINVAL;
	}

	if (gpio_is_valid(pdata->rst_gpio)) {
		dev_dbg(&client->dev, "reset_gpio=%d", pdata->rst_gpio);
		gpio_request(pdata->rst_gpio, NULL);
		gpio_direction_output(pdata->rst_gpio, 1);
		mdelay(10);
		gpio_direction_output(pdata->rst_gpio, 0);
		mdelay(20);
		gpio_direction_output(pdata->rst_gpio, 1);
		mdelay(10);
		gpio_free(pdata->rst_gpio);
	}
	else {	
		dev_err(&client->dev,
			"reset_gpio(%d) is invalid.\n", pdata->rst_gpio);
		kfree(pdata);
		return -EINVAL;
	}

	return 0;
}
#endif /* CONFIG_OF */

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,cap_touch");
	if (node) {
		touch_irq = irq_of_parse_and_map(node, 0);
		ret =
			request_irq(touch_irq, tpd_interrupt_handler,
				IRQF_TRIGGER_FALLING, TPD_DEVICE, NULL);
			if (ret > 0)
				printk(KERN_ERR "tpd request_irq IRQ LINE NOT AVAILABLE!.");
	} else {
		printk(KERN_ERR "[%s] tpd request_irq can not find touch eint device node!.", __func__);
	}
	return 0;
}

static s32 tpd_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err = 0;
	int fw_err = 0;
	struct task_struct *thread = NULL;
	struct elan_ktf2k_i2c_platform_data *pdata;
	struct elan_ktf2k_ts_data *ts;

#ifdef CONFIG_OF
	err = elan_ktf2k_ts_probe_dt(client);
	if (err != 0) {
		dev_err(&client->dev,
			"elan_ktf2k_ts_probe_dt() failed.(%d)\n", err);
		if(err == -ENOMEM)
			goto err_check_functionality_failed;
		else
			goto err_alloc_data_failed;
	}
#endif /* CONFIG_OF */

	ts = kzalloc(sizeof(struct elan_ktf2k_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		printk(KERN_ERR "[elan] %s: allocate elan_ktf2k_ts_data failed\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->disable = 0;

	ts->client = client;
	i2c_set_clientdata(client, ts);
// james: maybe remove	
	pdata = client->dev.platform_data;
	if (likely(pdata != NULL)) {
		ts->intr_gpio = pdata->intr_gpio;
		ts->rst_gpio  = pdata->rst_gpio;
	}

	ts->orient = 0;

	elan_ktf2k_ts_hw_reset(client);
	fw_err = elan_ktf2k_ts_setup(client);
	if (fw_err < 0) {
		printk(KERN_INFO "No Elan chip inside\n");
//		fw_err = -ENODEV;
	}

	private_ts = ts;

	thread = kthread_run(elan_ktf2k_ts_work_func, 0, TPD_DEVICE);

	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		printk(KERN_ERR " failed to create kernel thread: %d", err);
		goto err_create_thread_failed;
	}

	ts->int_enable = true;
	tpd_irq_registration();

	tpd_load_status = 1;

	printk(KERN_INFO "elan_ktf2k_ts_probe success\n");

	return 0;

err_create_thread_failed:
	kfree(ts);

err_alloc_data_failed:
#ifdef CONFIG_OF
	kfree(client->dev.platform_data);
	client->dev.platform_data = NULL;
#endif /* CONFIG_OF */

err_check_functionality_failed:

	return err;
}

static int tpd_i2c_remove(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);

	kfree(ts);

#ifdef CONFIG_OF
	kfree(client->dev.platform_data);
	client->dev.platform_data = NULL;
#endif /* CONFIG_OF */

	return 0;
}

static const struct i2c_device_id elan_ktf2k_ts_id[] = {
	{ ELAN_KTF2K_NAME, 0 },
	{ }
};

static struct i2c_driver tpd_i2c_driver = {
	.probe = tpd_i2c_probe,
	.remove = tpd_i2c_remove,
	.id_table = elan_ktf2k_ts_id,
	.driver = {
		.name = ELAN_KTF2K_NAME,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(elan_ktf2k_of_idtable),
#endif /* CONFIG_OF */
	},
};

static int tpd_local_init(void)
{
	int retval;

	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	retval = regulator_set_voltage(tpd->reg, 3300000, 3300000);
	if (retval != 0) {
		TPD_DMESG("Failed to set reg-vgp6 voltage: %d\n", retval);
		return -1;
	}

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		printk(KERN_INFO "unable to add i2c driver.");
		return -1;
	}

#ifdef ELAN_TEN_FINGERS
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (10-1), 0, 0);
#else
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (5-1), 0, 0);
#endif

	/* set vendor string */
	tpd->dev->id.vendor = 0x00;
	tpd->dev->id.product = 0x00;
	tpd->dev->id.version = FW_VERSION;

	TPD_RES_X = X_RESOLUTION;
	TPD_RES_Y = Y_RESOLUTION;

	tpd_type_cap = 1;
	return 0;
}

s32 ektf2k_enter_sleep(struct i2c_client *client)
{
	s32 ret = 0;
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);

	ret = gpio_request(ts->rst_gpio, "tp_rst_n");
	if (ret) {
		printk("ERROR: failed to request reset gpio !!!\n");
		return ret;
	}
	gpio_direction_output(ts->rst_gpio, 0);
	gpio_free(ts->rst_gpio);

	return ret;
}

s32 ektf2k_wakeup_sleep(struct i2c_client *client)
{
	s32 ret = 0;

	elan_ktf2k_ts_hw_reset(client);

	return ret;
}

/* Function to manage low power suspend */
static void tpd_suspend(struct device *h)
{
	s32 ret = -1;

	mutex_lock(&i2c_access);

	disable_irq(touch_irq);

	tpd_halt = 1;
	mutex_unlock(&i2c_access);

	ret = ektf2k_enter_sleep(private_ts->client);
	if (ret < 0)
		printk(KERN_INFO "suspend failed.");
}

/* Function to manage power-on resume */
static void tpd_resume(struct device *h)
{
	s32 ret = -1;

	ret = ektf2k_wakeup_sleep(private_ts->client);

	if (ret < 0)
		printk(KERN_INFO "resume failed.");

	printk(KERN_INFO "wakeup sleep.");

	mutex_lock(&i2c_access);
	tpd_halt = 0;

	enable_irq(touch_irq);

	mutex_unlock(&i2c_access);
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "ektf2k",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
//	.attrs = {
//		.attr = ektf2k_attrs,
//		.num  = ARRAY_SIZE(ektf2k_attrs),
//	},
};

static int __init tpd_driver_init(void)
{
	printk(KERN_INFO "[elan] %s driver version 0x0005: Integrated 2, 5, and 10 fingers together and auto-mapping resolution\n", __func__);
	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		printk(KERN_INFO "add generic driver failed");

	return 0;
}

static void __exit tpd_driver_exit(void)
{
	tpd_driver_remove(&tpd_device_driver);
	return;
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

MODULE_DESCRIPTION("ELAN KTF2K Touchscreen Driver");
MODULE_LICENSE("GPL");
