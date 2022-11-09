/* 
 *  mma8x5x.c - Linux kernel modules for 3-Axis Orientation/Motion
 *  Detection Sensor MMA8451/MMA8452/MMA8453/MMA8652/MMA8653 *
 *
 *  Copyright (C) 2012-2016 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  Copyright (C) 2018 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * Changelog: Change Code to Mediatek API
 *
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
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>

#include <cust_acc.h>
#include <hwmsensor.h>
#include <accel.h>

/*---------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
//#define CONFIG_MMA8X5X_LOWPASS   /*apply low pass filter on output*/
#define C_MAX_FIR_LENGTH (32)

#define MMA8X5X_BUFSIZE 60

#define MMA8X5X_SUCCESS				0
#define MMA8X5X_ERR_I2C				-1
#define MMA8X5X_ERR_STATUS			-3
#define MMA8X5X_ERR_SETUP_FAILURE	-4
#define MMA8X5X_ERR_GETGSENSORDATA	-5
#define MMA8X5X_ERR_IDENTIFICATION	-6

#define MMA8451_ID                      0x1A
#define MMA8452_ID                      0x2A
#define MMA8453_ID                      0x3A
#define MMA8652_ID                      0x4A
#define MMA8653_ID                      0x5A

static int mma8x5x_chip_id[] = {
	MMA8451_ID,
	MMA8452_ID,
	MMA8453_ID,
	MMA8652_ID,
	MMA8653_ID,
};

enum {
	MMA8X5X_STATUS = 0x00,
	MMA8X5X_OUT_X_MSB,
	MMA8X5X_OUT_X_LSB,
	MMA8X5X_OUT_Y_MSB,
	MMA8X5X_OUT_Y_LSB,
	MMA8X5X_OUT_Z_MSB,
	MMA8X5X_OUT_Z_LSB,

	MMA8X5X_F_SETUP = 0x09,
	MMA8X5X_TRIG_CFG,
	MMA8X5X_SYSMOD,
	MMA8X5X_INT_SOURCE,
	MMA8X5X_WHO_AM_I,
	MMA8X5X_XYZ_DATA_CFG,
	MMA8X5X_HP_FILTER_CUTOFF,

	MMA8X5X_PL_STATUS,
	MMA8X5X_PL_CFG,
	MMA8X5X_PL_COUNT,
	MMA8X5X_PL_BF_ZCOMP,
	MMA8X5X_P_L_THS_REG,

	MMA8X5X_FF_MT_CFG,
	MMA8X5X_FF_MT_SRC,
	MMA8X5X_FF_MT_THS,
	MMA8X5X_FF_MT_COUNT,

	MMA8X5X_TRANSIENT_CFG = 0x1D,
	MMA8X5X_TRANSIENT_SRC,
	MMA8X5X_TRANSIENT_THS,
	MMA8X5X_TRANSIENT_COUNT,

	MMA8X5X_PULSE_CFG,
	MMA8X5X_PULSE_SRC,
	MMA8X5X_PULSE_THSX,
	MMA8X5X_PULSE_THSY,
	MMA8X5X_PULSE_THSZ,
	MMA8X5X_PULSE_TMLT,
	MMA8X5X_PULSE_LTCY,
	MMA8X5X_PULSE_WIND,

	MMA8X5X_ASLP_COUNT,
	MMA8X5X_CTRL_REG1,
	MMA8X5X_CTRL_REG2,
	MMA8X5X_CTRL_REG3,
	MMA8X5X_CTRL_REG4,
	MMA8X5X_CTRL_REG5,

	MMA8X5X_OFF_X,
	MMA8X5X_OFF_Y,
	MMA8X5X_OFF_Z,

	MMA8X5X_REG_END,
};


#define MMA8X5X_ACC_RANGE_2g 0
#define MMA8X5X_ACC_RANGE_4g 1
#define MMA8X5X_ACC_RANGE_8g 2

#define MMA8X5X_ACC_SENSITIVITY_2G	0x4000
#define MMA8X5X_ACC_SENSITIVITY_4G	0x2000
#define MMA8X5X_ACC_SENSITIVITY_8G	0x1000

#define MODE_CHANGE_DELAY_MS    100

#define MMA8X5X_ACC_ODR_50HZ		4
#define MMA8X5X_ACC_ODR_100HZ		3
#define MMA8X5X_ACC_ODR_200HZ		2

/*----------------------------------------------------------------------------*/
#define MMA8X5X_AXIS_X          0
#define MMA8X5X_AXIS_Y          1
#define MMA8X5X_AXIS_Z          2
#define MMA8X5X_AXES_NUM        3
#define MMA8X5X_DATA_LEN        6
#define MMA8X5X_DEV_NAME        "MMA8X5X_ACCEL"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mmx8x5x_i2c_id[] = {{MMA8X5X_DEV_NAME,0},{}};
static int mmx8x5x_init_flag = -1;

/*----------------------------------------------------------------------------*/
typedef enum {
	ADX_TRC_FILTER  = 0x01,
	ADX_TRC_RAWDATA = 0x02,
	ADX_TRC_IOCTL   = 0x04,
	ADX_TRC_CALI    = 0x08,
	ADX_TRC_INFO    = 0x10,
} ADX_TRC;
/*----------------------------------------------------------------------------*/
typedef enum {
	ACCEL_TRC_FILTER  = 0x01,
	ACCEL_TRC_RAWDATA = 0x02,
	ACCEL_TRC_IOCTL   = 0x04,
	ACCEL_TRC_CALI    = 0x08,
	ACCEL_TRC_INFO    = 0x10,
	ACCEL_TRC_DATA    = 0x20,
} ACCEL_TRC;
/*----------------------------------------------------------------------------*/
struct data_filter {
    s16 raw[C_MAX_FIR_LENGTH][MMA8X5X_AXES_NUM];
    int sum[MMA8X5X_AXES_NUM];
    int num;
    int idx;
};

struct acc_hw acc_cust;
static struct acc_hw *hw = &acc_cust;
/* For  driver get cust info */
struct acc_hw *get_cust_acc(void)
{
	return &acc_cust;
}

/*----------------------------------------------------------------------------*/
struct mmx8x5x_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert	cvt;
	atomic_t 				layout;
	/*misc*/
	atomic_t				trace;
	atomic_t				suspend;
	atomic_t				selftest;
	atomic_t				filter;
	s16						cali_sw[MMA8X5X_AXES_NUM+1];

	/*data*/
	s8						offset[MMA8X5X_AXES_NUM+1];  /*+1: for 4-byte alignment*/
	s16						data[MMA8X5X_AXES_NUM+1];

	int 					sensitivity;
	u8 						sample_rate;

#if defined(CONFIG_MMA8X5X_LOWPASS)
	atomic_t				firlen;
	atomic_t				fir_en;
	struct data_filter		fir;
#endif 
};
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int mmx8x5x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int mmx8x5x_i2c_remove(struct i2c_client *client);
static int mmx8x5x_init_client(struct i2c_client *client, bool enable);
static int mmx8x5x_SetPowerMode(struct i2c_client *client, bool enable);

static int mmx8x5x_ReadAccRawData(struct i2c_client *client, s16 data[MMA8X5X_AXES_NUM]);
static int mmx8x5x_suspend(struct device *dev);
static int mmx8x5x_resume(struct device *dev);
static int mmx8x5x_SetSampleRate(struct i2c_client *client, u8 sample_rate);

static int mmx8x5x_local_init(void);
static int mmx8x5x_local_uninit(void);

static DEFINE_MUTEX(mmx8x5x_init_mutex);

#ifdef CONFIG_OF
static const struct of_device_id accel_of_match[] = {
	{.compatible = "mediatek,gsensor"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops mmx8x5x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mmx8x5x_suspend, mmx8x5x_resume)
};
#endif

static struct i2c_driver mmx8x5x_i2c_driver = {
	.probe		= mmx8x5x_i2c_probe,
	.remove		= mmx8x5x_i2c_remove,
	.id_table	= mmx8x5x_i2c_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= MMA8X5X_DEV_NAME,
#ifdef CONFIG_OF
	.of_match_table = accel_of_match, //need add in dtsi first
#endif
#ifdef CONFIG_PM_SLEEP
		.pm = &mmx8x5x_pm_ops,
#endif
	},
};

static struct acc_init_info  mmx8x5x_init_info = {
	.name   = MMA8X5X_DEV_NAME,
	.init   = mmx8x5x_local_init,
	.uninit = mmx8x5x_local_uninit,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *mmx8x5x_i2c_client = NULL;

static struct mmx8x5x_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;
static bool enable_status = false;

/*----------------------------------------------------------------------------*/

#define GSE_TAG                  "[accel] "

#define GSE_FUN(f)               pr_debug(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    pr_err(KERN_ERR GSE_TAG "%s %d : " fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    pr_debug(KERN_INFO GSE_TAG "%s %d : " fmt, __FUNCTION__, __LINE__, ##args)

static struct i2c_msg msgs[2];
static u8 read_addr;

static int mpu_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err = 0;
	read_addr = addr;

	mutex_lock(&mmx8x5x_init_mutex);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &read_addr;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&mmx8x5x_init_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&mmx8x5x_init_mutex);
		GSE_ERR("length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else
		err = 0;
	mutex_unlock(&mmx8x5x_init_mutex);
	return err;

}

static char writebuf[C_I2C_FIFO_SIZE];

static int mpu_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{	/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err = 0;
	int idx = 0;
	int num = 0;

	mutex_lock(&mmx8x5x_init_mutex);
	if (!client) {
		mutex_unlock(&mmx8x5x_init_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		mutex_unlock(&mmx8x5x_init_mutex);
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	num = 0;
	writebuf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		writebuf[num++] = data[idx];

	err = i2c_master_send(client, writebuf, num);
	if (err < 0) {
		mutex_unlock(&mmx8x5x_init_mutex);
		GSE_ERR("i2c send command error!!\n");
		return -EFAULT;
	}
	mutex_unlock(&mmx8x5x_init_mutex);
	return err;
}

int mmx8x5x_hwmsen_read_block(u8 addr, u8 *buf, u8 len)
{
	if (NULL == mmx8x5x_i2c_client) {
		GSE_ERR("MMA8X5X_hwmsen_read_block null ptr!!\n");
		return MMA8X5X_ERR_I2C;
	}
	return mpu_i2c_read_block(mmx8x5x_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(mmx8x5x_hwmsen_read_block);

int mmx8x5x_hwmsen_write_block(u8 addr, u8 *buf, u8 len)
{
	if (NULL == mmx8x5x_i2c_client) {
		GSE_ERR("MMA8X5X_hwmsen_write_block null ptr!!\n");
		return MMA8X5X_ERR_I2C;
	}
	return mpu_i2c_write_block(mmx8x5x_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(mmx8x5x_hwmsen_write_block);

/*----------------------------------------------------------------------------*/
static void mmx8x5x_dumpReg(struct i2c_client *client)
{
	int i;
	u8 addr = 0x0;
	u8 regdata = 0;
	for(i = 0; i < 25 ; i++)
	{
		//dump all
		mpu_i2c_read_block(client,addr,&regdata, 0x01);
		GSE_LOG("Reg addr=%x regdata=%x\n",addr,regdata);
		addr++;
	}
}
/*----------------------------------------------------------------------------*/

static int mmx8x5x_write_rel_calibration(struct mmx8x5x_i2c_data *obj, int dat[MMA8X5X_AXES_NUM])
{
	obj->cali_sw[MMA8X5X_AXIS_X] = obj->cvt.sign[MMA8X5X_AXIS_X]*dat[obj->cvt.map[MMA8X5X_AXIS_X]];
	obj->cali_sw[MMA8X5X_AXIS_Y] = obj->cvt.sign[MMA8X5X_AXIS_Y]*dat[obj->cvt.map[MMA8X5X_AXIS_Y]];
	obj->cali_sw[MMA8X5X_AXIS_Z] = obj->cvt.sign[MMA8X5X_AXIS_Z]*dat[obj->cvt.map[MMA8X5X_AXIS_Z]];
#if DEBUG
		if(atomic_read(&obj->trace) & ACCEL_TRC_CALI)
		{
			GSE_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
				obj->cvt.sign[MMA8X5X_AXIS_X],obj->cvt.sign[MMA8X5X_AXIS_Y],obj->cvt.sign[MMA8X5X_AXIS_Z],
				dat[MMA8X5X_AXIS_X], dat[MMA8X5X_AXIS_Y], dat[MMA8X5X_AXIS_Z],
				obj->cvt.map[MMA8X5X_AXIS_X],obj->cvt.map[MMA8X5X_AXIS_Y],obj->cvt.map[MMA8X5X_AXIS_Z]);
			GSE_LOG("write calibration data  (%5d, %5d, %5d)\n", 
				obj->cali_sw[MMA8X5X_AXIS_X],obj->cali_sw[MMA8X5X_AXIS_Y],obj->cali_sw[MMA8X5X_AXIS_Z]);
		}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_ResetCalibration(struct i2c_client *client)
{
	struct mmx8x5x_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_ReadCalibration(struct i2c_client *client, int dat[MMA8X5X_AXES_NUM])
{
	struct mmx8x5x_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[MMA8X5X_AXIS_X]] = obj->cvt.sign[MMA8X5X_AXIS_X]*obj->cali_sw[MMA8X5X_AXIS_X];
	dat[obj->cvt.map[MMA8X5X_AXIS_Y]] = obj->cvt.sign[MMA8X5X_AXIS_Y]*obj->cali_sw[MMA8X5X_AXIS_Y];
	dat[obj->cvt.map[MMA8X5X_AXIS_Z]] = obj->cvt.sign[MMA8X5X_AXIS_Z]*obj->cali_sw[MMA8X5X_AXIS_Z];

#if DEBUG
		if(atomic_read(&obj->trace) & ACCEL_TRC_CALI)
		{
			GSE_LOG("Read calibration data  (%5d, %5d, %5d)\n",
				dat[MMA8X5X_AXIS_X],dat[MMA8X5X_AXIS_Y],dat[MMA8X5X_AXIS_Z]);
		}
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/

static int mmx8x5x_WriteCalibration(struct i2c_client *client, int dat[MMA8X5X_AXES_NUM])
{
	struct mmx8x5x_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[MMA8X5X_AXES_NUM];

	GSE_FUN();
	if(!obj || ! dat)
	{
		GSE_ERR("null ptr!!\n");
		return -EINVAL;
	}
	else
	{
		cali[obj->cvt.map[MMA8X5X_AXIS_X]] = obj->cvt.sign[MMA8X5X_AXIS_X]*obj->cali_sw[MMA8X5X_AXIS_X];
		cali[obj->cvt.map[MMA8X5X_AXIS_Y]] = obj->cvt.sign[MMA8X5X_AXIS_Y]*obj->cali_sw[MMA8X5X_AXIS_Y];
		cali[obj->cvt.map[MMA8X5X_AXIS_Z]] = obj->cvt.sign[MMA8X5X_AXIS_Z]*obj->cali_sw[MMA8X5X_AXIS_Z];
		cali[MMA8X5X_AXIS_X] += dat[MMA8X5X_AXIS_X];
		cali[MMA8X5X_AXIS_Y] += dat[MMA8X5X_AXIS_Y];
		cali[MMA8X5X_AXIS_Z] += dat[MMA8X5X_AXIS_Z];
#if DEBUG
		if(atomic_read(&obj->trace) & ACCEL_TRC_CALI)
		{
			GSE_LOG("write accel calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
				dat[MMA8X5X_AXIS_X], dat[MMA8X5X_AXIS_Y], dat[MMA8X5X_AXIS_Z],
				cali[MMA8X5X_AXIS_X],cali[MMA8X5X_AXIS_Y],cali[MMA8X5X_AXIS_Z]);
		}
#endif
		return mmx8x5x_write_rel_calibration(obj, cali);
	}

	return err;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf;
	int res = 0;
	int i = 0;

	res = mpu_i2c_read_block(client, MMA8X5X_WHO_AM_I, &databuf, 0x1);
	if (res < 0)
	{
		return MMA8X5X_ERR_I2C;
	}

	GSE_LOG(" MMA8X5X  id %x!\n",databuf);

	for (i = 0; i < sizeof(mma8x5x_chip_id) /
			sizeof(mma8x5x_chip_id[0]); i++)
		if (databuf == mma8x5x_chip_id[i])
			return MMA8X5X_SUCCESS;

	return MMA8X5X_ERR_IDENTIFICATION;
}

static int mmx8x5x_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};
	int res = 0;

	if(enable == sensor_power)
	{
		GSE_LOG("Sensor power status is newest!\n");
		return MMA8X5X_SUCCESS;
	}

	if(mpu_i2c_read_block(client, MMA8X5X_CTRL_REG1, databuf, 0x01))
	{
		GSE_ERR("read mmx8x5x power ctl register err!\n");
		return MMA8X5X_ERR_I2C;
	}
	GSE_LOG("MMA8X5X_CTRL_REG1:databuf[0] =  %x!\n", databuf[0]);

	if(true == enable)
	{
		databuf[0] |= 0x01;
	}
	else
	{
		databuf[0] &= 0xfe;
	}

	res = mpu_i2c_write_block(client, MMA8X5X_CTRL_REG1, databuf, 0x1);
	if(res < 0)
	{
		GSE_LOG("MMA8X5X set power mode failed!\n");
		return MMA8X5X_ERR_I2C;
	}
	else
	{
		GSE_LOG("set MMA8X5X  power mode ok %d!\n", enable);
	}

	sensor_power = enable;

	return MMA8X5X_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_SetFullScale(struct i2c_client *client, u8 acc_fs)
{
	u8 databuf[2] = {0};
	int res = 0;
	struct mmx8x5x_i2c_data *obj = i2c_get_clientdata(client);

	GSE_FUN();

	databuf[0] = acc_fs;

	res = mpu_i2c_write_block(client, MMA8X5X_XYZ_DATA_CFG, databuf, 0x1);
	if(res < 0)
	{
		GSE_ERR("write full scale register err!\n");
		return MMA8X5X_ERR_I2C;
	}
	switch(acc_fs)
	{
		case MMA8X5X_ACC_RANGE_2g:
			obj->sensitivity = MMA8X5X_ACC_SENSITIVITY_2G;
			break;
		case MMA8X5X_ACC_RANGE_4g:
			obj->sensitivity = MMA8X5X_ACC_SENSITIVITY_4G;
			break;
		case MMA8X5X_ACC_RANGE_8g:
			obj->sensitivity = MMA8X5X_ACC_SENSITIVITY_8G;
			break;
		default:
			obj->sensitivity = MMA8X5X_ACC_SENSITIVITY_2G;
			break;
	}

	msleep(MODE_CHANGE_DELAY_MS);

	return MMA8X5X_SUCCESS;
}

/*----------------------------------------------------------------------------*/
// set the acc sample rate
static int mmx8x5x_SetSampleRate(struct i2c_client *client, u8 sample_rate)
{
	u8 databuf[2] = {0};
	int res = 0;
	GSE_FUN();

	res = mmx8x5x_SetPowerMode(client, true);	//set Sample Rate will enable power and should changed power status
	if(res != MMA8X5X_SUCCESS)
	{
		return res;
	}

	if(mpu_i2c_read_block(client, MMA8X5X_CTRL_REG1, databuf, 0x01))
	{
		GSE_ERR("read acc data format register err!\n");
		return MMA8X5X_ERR_I2C;
	}
	else
	{
		GSE_LOG("read  acc data format register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= ~(0x07 << 3);//clear 
	databuf[0] |= (sample_rate << 3);

	res = mpu_i2c_write_block(client, MMA8X5X_CTRL_REG1, databuf, 0x1);
	if(res < 0)
	{
		GSE_ERR("write sample rate register err!\n");
		return MMA8X5X_ERR_I2C;
	}

	return MMA8X5X_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_ReadAccData(struct i2c_client *client, char *buf, int bufsize)
{
	struct mmx8x5x_i2c_data *obj = (struct mmx8x5x_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[MMA8X5X_AXES_NUM];
	int res = 0;
	memset(databuf, 0, sizeof(u8)*10);

	if(NULL == buf)
	{
		return -1;
	}
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	if(sensor_power == false)
	{
		res = mmx8x5x_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on mmx8x5x error %d!\n", res);
		}
		msleep(20);
	}

	res = mmx8x5x_ReadAccRawData(client, obj->data);
	if(res < 0)
	{
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
		obj->data[MMA8X5X_AXIS_X] = (long)(obj->data[MMA8X5X_AXIS_X]) * GRAVITY_EARTH_1000 / obj->sensitivity;
		obj->data[MMA8X5X_AXIS_Y] = (long)(obj->data[MMA8X5X_AXIS_Y]) * GRAVITY_EARTH_1000 / obj->sensitivity;
		obj->data[MMA8X5X_AXIS_Z] = (long)(obj->data[MMA8X5X_AXIS_Z]) * GRAVITY_EARTH_1000 / obj->sensitivity;

		obj->data[MMA8X5X_AXIS_X] += obj->cali_sw[MMA8X5X_AXIS_X];
		obj->data[MMA8X5X_AXIS_Y] += obj->cali_sw[MMA8X5X_AXIS_Y];
		obj->data[MMA8X5X_AXIS_Z] += obj->cali_sw[MMA8X5X_AXIS_Z];

		/*remap coordinate*/
		acc[obj->cvt.map[MMA8X5X_AXIS_X]] = obj->cvt.sign[MMA8X5X_AXIS_X]*obj->data[MMA8X5X_AXIS_X];
		acc[obj->cvt.map[MMA8X5X_AXIS_Y]] = obj->cvt.sign[MMA8X5X_AXIS_Y]*obj->data[MMA8X5X_AXIS_Y];
		acc[obj->cvt.map[MMA8X5X_AXIS_Z]] = obj->cvt.sign[MMA8X5X_AXIS_Z]*obj->data[MMA8X5X_AXIS_Z];

		sprintf(buf, "%04x %04x %04x", acc[MMA8X5X_AXIS_X], acc[MMA8X5X_AXIS_Y], acc[MMA8X5X_AXIS_Z]);

		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			GSE_LOG("raw data:obj->data:%04x %04x %04x\n", obj->data[MMA8X5X_AXIS_X], obj->data[MMA8X5X_AXIS_Y], obj->data[MMA8X5X_AXIS_Z]);
			GSE_LOG("acc:%04x %04x %04x\n", acc[MMA8X5X_AXIS_X], acc[MMA8X5X_AXIS_Y], acc[MMA8X5X_AXIS_Z]);
		}
	}

	return 0;
}
static int mmx8x5x_ReadAccRawData(struct i2c_client *client, s16 data[MMA8X5X_AXES_NUM])
{
	int err = 0;
	char databuf[6] = {0};

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else
	{
		if(mpu_i2c_read_block(client, MMA8X5X_OUT_X_MSB, databuf, 6))
		{
			GSE_ERR("MMA8X5X read acc data  error\n");
			return -2;
		}
		else
		{
			data[MMA8X5X_AXIS_X] = (s16)((databuf[MMA8X5X_AXIS_X*2] << 8) | (databuf[MMA8X5X_AXIS_X*2+1]));
			data[MMA8X5X_AXIS_Y] = (s16)((databuf[MMA8X5X_AXIS_Y*2] << 8) | (databuf[MMA8X5X_AXIS_Y*2+1]));
			data[MMA8X5X_AXIS_Z] = (s16)((databuf[MMA8X5X_AXIS_Z*2] << 8) | (databuf[MMA8X5X_AXIS_Z*2+1]));
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8)*10);

	if((NULL == buf) || (bufsize<=30))
	{
		return -1;
	}

	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "MMA8X5X Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mmx8x5x_i2c_client;
	char strbuf[MMA8X5X_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	mmx8x5x_ReadChipInfo(client, strbuf, MMA8X5X_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mmx8x5x_i2c_client;
	char strbuf[MMA8X5X_BUFSIZE];
	int x,y,z;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	mmx8x5x_ReadAccData(client, strbuf, MMA8X5X_BUFSIZE);
	sscanf(strbuf, "%x %x %x", &x, &y, &z);
	return snprintf(buf, PAGE_SIZE, "%d, %d, %d\n", x,y,z);
}

static ssize_t show_sensorrawdata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mmx8x5x_i2c_client;
	s16 data[MMA8X5X_AXES_NUM] = {0};

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	mmx8x5x_ReadAccRawData(client, data);

	return snprintf(buf, PAGE_SIZE, "%x,%x,%x\n", data[0],data[1],data[2]);            
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mmx8x5x_i2c_data *obj = obj_i2c_data;

	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct mmx8x5x_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return count;
	}

	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}
	else
	{
		GSE_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}

	return count;
}

static ssize_t show_chipinit_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mmx8x5x_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));

	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_chipinit_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct mmx8x5x_i2c_data *obj = obj_i2c_data;

	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return count;
	}

	mmx8x5x_init_client(obj->client, true);
	mmx8x5x_dumpReg(obj->client);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct mmx8x5x_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	if(obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: i2c_num=%d, direction=%d, (power_id=%d, power_vol=%d)\n",
	            obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
		mmx8x5x_dumpReg(obj->client);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}

	return len;
}
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct mmx8x5x_i2c_data *data = obj_i2c_data;
	if(NULL == data)
	{
		GSE_ERR("mmx8x5x_i2c_data is null!!\n");
		return -1;
	}

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int layout = 0;
	struct mmx8x5x_i2c_data *data = obj_i2c_data;

	if(NULL == data)
	{
		GSE_ERR("mmx8x5x_i2c_data is null!!\n");
		return count;
	}

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			GSE_ERR("HWMSEN_GET_CONVERT function error!\r\n");
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			GSE_ERR("invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		}
		else
		{
			GSE_ERR("invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	}
	else
	{
		GSE_ERR("invalid format = '%s'\n", buf);
	}

	return count;
}

/*----------------------------------------------------------------------------*/

static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensorrawdata, S_IRUGO, show_sensorrawdata_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(chipinit, S_IWUSR | S_IRUGO, show_chipinit_value, store_chipinit_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(layout, S_IRUGO | S_IWUSR, show_layout_value, store_layout_value);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *mmx8x5x_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_sensorrawdata,   /*dump sensor raw data*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_chipinit,
	&driver_attr_layout,
};
/*----------------------------------------------------------------------------*/
static int mmx8x5x_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(mmx8x5x_attr_list)/sizeof(mmx8x5x_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(0 != (err = driver_create_file(driver,  mmx8x5x_attr_list[idx])))
		{
			GSE_ERR("driver_create_file (%s) = %d\n",  mmx8x5x_attr_list[idx]->attr.name, err);
			break;
		}
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int mmx8x5x_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof( mmx8x5x_attr_list)/sizeof( mmx8x5x_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver,  mmx8x5x_attr_list[idx]);
	}

	return err;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_init_client(struct i2c_client *client, bool enable)
{
	struct mmx8x5x_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	GSE_FUN();
	GSE_LOG(" mmx8x5x addr %x!\n",client->addr);
	res = mmx8x5x_CheckDeviceID(client);
	if(res != MMA8X5X_SUCCESS)
	{
		return res;
	}

//	res = mmx8x5x_SetFullScale(client,MMA8X5X_ACC_RANGE_2g);
	res = mmx8x5x_SetFullScale(client,MMA8X5X_ACC_RANGE_4g);
	if(res != MMA8X5X_SUCCESS)
	{
		return res;
	}

	res = mmx8x5x_SetSampleRate(client, obj->sample_rate);
	if(res != MMA8X5X_SUCCESS )
	{
		return res;
	}
	
	res = mmx8x5x_SetPowerMode(client, enable);
	if(res != MMA8X5X_SUCCESS)
	{
		return res;
	}

	GSE_LOG("mmx8x5x_init_client OK!\n");
	//acc setting

#ifdef CONFIG_MMA8X5X_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return MMA8X5X_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int mmx8x5x_open_report_data(int open)
{
	return 0;
}

// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL
static int mmx8x5x_enable_nodata(int en)
{
	int value = en;
	int err = 0;
	struct mmx8x5x_i2c_data *priv = obj_i2c_data;

	if(priv == NULL)
	{
		GSE_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}

	if(value == 1)
	{
		enable_status = true;
	}
	else
	{
		enable_status = false;
		priv->sample_rate = 100; //default rate
	}
	GSE_LOG("enable value=%d, sensor_power =%d\n",value,sensor_power);

	if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
	{
		GSE_LOG("Gsensor device have updated!\n");
	}
	else
	{
		err = mmx8x5x_SetPowerMode( priv->client, enable_status);
	}

	GSE_LOG("%s OK!\n",__FUNCTION__);
	return err;
}

static int mmx8x5x_set_delay(u64 ns)
{
	int value =0;
	int err = 0;
	u8 sample_delay = 0;
	struct mmx8x5x_i2c_data *priv = obj_i2c_data;
	value = (int)ns/1000/1000;

	if(priv == NULL)
	{
		GSE_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}

	if(value <= 5)
	{
		sample_delay = MMA8X5X_ACC_ODR_200HZ;
	}
	else if(value <= 10)
	{
		sample_delay = MMA8X5X_ACC_ODR_100HZ;
	}
	else
	{
		sample_delay = MMA8X5X_ACC_ODR_50HZ;
	}
	priv->sample_rate = sample_delay;
	err = mmx8x5x_SetSampleRate(priv->client, sample_delay);
	if(err != MMA8X5X_SUCCESS )
	{
		GSE_ERR("Set delay parameter error!\n");
	}

	if(value >= 50)
	{
		atomic_set(&priv->filter, 0);
	}
	else
	{
#if defined(CONFIG_MMA8X5X_LOWPASS)
		priv->fir.num = 0;
		priv->fir.idx = 0;
		priv->fir.sum[MMA8X5X_AXIS_X] = 0;
		priv->fir.sum[MMA8X5X_AXIS_Y] = 0;
		priv->fir.sum[MMA8X5X_AXIS_Z] = 0;
#endif
		atomic_set(&priv->filter, 1);
	}

	return 0;
}

static int mmx8x5x_get_data(int* x ,int* y,int* z, int* status)
{
	char buff[MMA8X5X_BUFSIZE];
	struct mmx8x5x_i2c_data *priv = obj_i2c_data;

	if(priv == NULL)
	{
		GSE_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}
	if(atomic_read(&priv->trace) & ACCEL_TRC_DATA)
	{
		GSE_LOG("%s (%d),  \n",__FUNCTION__,__LINE__);
	}
	memset(buff, 0, sizeof(buff));
	mmx8x5x_ReadAccData(priv->client, buff, MMA8X5X_BUFSIZE);

	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

    return 0;
}


/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int mmx8x5x_open(struct inode *inode, struct file *file)
{
	file->private_data = mmx8x5x_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int mmx8x5x_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long mmx8x5x_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct mmx8x5x_i2c_data *obj = (struct mmx8x5x_i2c_data*)i2c_get_clientdata(client);
	char strbuf[MMA8X5X_BUFSIZE];
	void __user *data;
	struct SENSOR_DATA sensor_data;
	int err = 0;
	int cali[3];

	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case GSENSOR_IOCTL_INIT:
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}

			mmx8x5x_ReadChipInfo(client, strbuf, MMA8X5X_BUFSIZE);

			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;
			}
			break;

		case GSENSOR_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}

			mmx8x5x_ReadAccData(client, strbuf, MMA8X5X_BUFSIZE);

			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;
			}
			break;

		case GSENSOR_IOCTL_READ_GAIN:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}
			break;

		case GSENSOR_IOCTL_READ_OFFSET:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}
			break;

		case GSENSOR_IOCTL_READ_RAW_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}

			mmx8x5x_ReadAccRawData(client, (s16 *)strbuf);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;
			}
			break;

		case GSENSOR_IOCTL_SET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}
			if(copy_from_user(&sensor_data, data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}
			if(atomic_read(&obj->suspend))
			{
				GSE_ERR("Perform calibration in suspend state!!\n");
				err = -EINVAL;
			}
			else
			{
				cali[MMA8X5X_AXIS_X] = (s64)(sensor_data.x);
				cali[MMA8X5X_AXIS_Y] = (s64)(sensor_data.y);
				cali[MMA8X5X_AXIS_Z] = (s64)(sensor_data.z);
				err = mmx8x5x_WriteCalibration(client, cali);
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = mmx8x5x_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}
			err = mmx8x5x_ReadCalibration(client, cali);
			if(err < 0)
			{
				break;
			}

			sensor_data.x = (s64)(cali[MMA8X5X_AXIS_X]);
			sensor_data.y = (s64)(cali[MMA8X5X_AXIS_Y]);
			sensor_data.z = (s64)(cali[MMA8X5X_AXIS_Z]);

			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}
			break;

		default:
			GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	return err;
}
#ifdef CONFIG_COMPAT
static long mmx8x5x_compat_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd)
	{
		case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
			if (arg32 == NULL)
			{
				err = -EINVAL;
				break;
			}

			err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
			if (err){
				GSE_ERR("GSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
				return err;
			}
			break;

		case COMPAT_GSENSOR_IOCTL_SET_CALI:
			if (arg32 == NULL)
			{
				err = -EINVAL;
				break;
			}

			err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_SET_CALI, (unsigned long)arg32);
			if (err){
				GSE_ERR("GSENSOR_IOCTL_SET_CALI unlocked_ioctl failed.");
				return err;
			}
			break;

		case COMPAT_GSENSOR_IOCTL_GET_CALI:
			if (arg32 == NULL)
			{
				err = -EINVAL;
				break;
			}

			err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_GET_CALI, (unsigned long)arg32);
			if (err){
				GSE_ERR("GSENSOR_IOCTL_GET_CALI unlocked_ioctl failed.");
			return err;
			}
			break;

		case COMPAT_GSENSOR_IOCTL_CLR_CALI:
			if (arg32 == NULL)
			{
				err = -EINVAL;
				break;
			}

			err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_CLR_CALI, (unsigned long)arg32);
			if (err){
				GSE_ERR("GSENSOR_IOCTL_CLR_CALI unlocked_ioctl failed.");
				return err;
			}
			break;

		default:
			GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	return err;
}
#endif

/*----------------------------------------------------------------------------*/
static struct file_operations mmx8x5x_acc_fops = {
	.owner = THIS_MODULE,
	.open = mmx8x5x_open,
	.release = mmx8x5x_release,
	.unlocked_ioctl = mmx8x5x_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mmx8x5x_compat_ioctl,
#endif
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mmx8x5x_acc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &mmx8x5x_acc_fops,
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

static int mmx8x5x_suspend(struct device *dev) 
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mmx8x5x_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	atomic_set(&obj->suspend, 1);

	err = mmx8x5x_SetPowerMode(obj->client, false);
	if(err)
	{
		GSE_ERR("write power control fail!!\n");
		return err;
	}

	sensor_power = false;

	return err;
}
/*----------------------------------------------------------------------------*/
static int mmx8x5x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mmx8x5x_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -1;
	}

	err = mmx8x5x_SetPowerMode(obj->client, enable_status);
	if(err)
	{
		GSE_ERR("initialize client fail! err code %d!\n", err);
		return err ;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int mmx8x5x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client = NULL;
	struct mmx8x5x_i2c_data *obj = NULL;
	struct acc_control_path ctl_path = { 0 };
	struct acc_data_path data_path = { 0 };
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(struct mmx8x5x_i2c_data));

	obj->hw = hw; 
	obj->sample_rate = MMA8X5X_ACC_ODR_100HZ;

	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if(err)
	{
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	mmx8x5x_i2c_client = new_client;
	err = mmx8x5x_init_client(new_client, false);
	if(err)
	{
		goto exit_init_failed;
	}

	err = misc_register(&mmx8x5x_acc_device);
	if(err)
	{
		GSE_ERR("mmx8x5x_acc_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}

	err = mmx8x5x_create_attr(&(mmx8x5x_init_info.platform_diver_addr->driver));
	if(err < 0)
	{
		goto exit_create_attr_failed;
	}

	ctl_path.open_report_data= mmx8x5x_open_report_data;
	ctl_path.enable_nodata = mmx8x5x_enable_nodata;
	ctl_path.set_delay  = mmx8x5x_set_delay;
	ctl_path.is_report_input_direct = false;
	ctl_path.is_support_batch = obj->hw->is_batch_supported;
	err = acc_register_control_path(&ctl_path);
	if(err)
	{
		 GSE_ERR("register acc control path err\n");
		 goto exit_init_failed;
	}

	data_path.get_data = mmx8x5x_get_data;
	data_path.vender_div = 1000;
	err = acc_register_data_path(&data_path);
	if(err)
	{
		GSE_ERR("register acc data path err= %d\n", err);
		goto exit_init_failed;
	}

	mmx8x5x_init_flag = 0;
	GSE_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	misc_deregister(&mmx8x5x_acc_device);
exit_misc_device_register_failed:
exit_init_failed:
exit_kfree:
	kfree(obj);
exit:
	mmx8x5x_init_flag = -1;
	GSE_ERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int mmx8x5x_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = mmx8x5x_delete_attr(&(mmx8x5x_init_info.platform_diver_addr->driver));
	if(err)
	{
		GSE_ERR("mmx8x5x_i2c_remove fail: %d\n", err);
	}

	misc_deregister(&mmx8x5x_acc_device);

	mmx8x5x_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/

static int mmx8x5x_local_uninit(void)
{
	i2c_del_driver(&mmx8x5x_i2c_driver);
	return 0;
}

static int mmx8x5x_local_init(void)
{
	if(i2c_add_driver(&mmx8x5x_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}

	if(mmx8x5x_init_flag == -1)
	{
		GSE_ERR("%s init failed!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


/*----------------------------------------------------------------------------*/
static int __init mmx8x5x_init(void)
{
	const char *name = "mediatek,mmx8x5x";

	hw = get_accel_dts_func(name, hw);
	if (!hw)
		GSE_ERR("get dts info fail\n");

	acc_driver_add(&mmx8x5x_init_info);

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit mmx8x5x_exit(void)
{
	GSE_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(mmx8x5x_init);
module_exit(mmx8x5x_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MMA8X5X Accelerometer");
MODULE_AUTHOR("stst");




