/*
 * Copyright (C) 2017 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *
 * Project:
 * --------
 *
 * Description:
 * ------------
 *   Image sensor driver function
 *
 * Author:
 * -------
 *
 *=============================================================
 *	       HISTORY
 * Below this line, this part is controlled by GCoreinc. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *
 * Changelog:
 *   2017/12/18 DatVT	First release. Add new image sensor.
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by GCoreinc. DO NOT MODIFY!!
 *=============================================================
 ******************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "sr200pc20m_mipi_Sensor.h"

//#define SENSOR_SETTING_DEBUG
//#define SENSOR_MIRROR_EFFECT_SUPPORT
//#define SENSOR_BINARY_EFFECT_SUPPORT

#define SR200PC20M_DEBUG	//Enable kernel log
#ifdef  SR200PC20M_DEBUG
#define SENSORDB printk
#else
#define SENSORDB(x, ...)
#endif

#define  SR200PC20M_TEST_PATTERN_CHECKSUM (0xffffff)	//Dummy value

static const SR200PC20M_REG REG_DEVID = {
	.addr = PAGE0,
	.data = SR200PC20M_REG_DEVID
};

static imgsensor_info_struct imgsensor_info = {
	.sensor_id = SR200PC20M_SENSOR_ID,	/* record sensor id defined in Kd_imgsensor.h */
	
	.checksum_value = SR200PC20M_TEST_PATTERN_CHECKSUM,	/* checksum value for Camera Auto Test */
	
	.pre = {
		.resolution = SR200PC20M_FMT_XGA,
		.pclk = 72000000,			/* record different mode's pclk */
		.linelength = 1024,			/* record different mode's linelength */
		.framelength = 768,		/* record different mode's framelength */
		.startx = 0,				/* record different mode's startx of grabwindow */
		.starty = 0,				/* record different mode's starty of grabwindow */
		.grabwindow_width = 1024,	/* record different mode's width of grabwindow */
		.grabwindow_height = 768,	/* record different mode's height of grabwindow */
		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/* following for GetDefaultFramerateByScenario() */
		.max_framerate = 30,
	},
	.cap = {
		.resolution = SR200PC20M_FMT_UXGA,
		.pclk = 72000000,
		.linelength = 1600,
		.framelength = 1200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 30,
	},
	.normal_video = {
		.resolution = SR200PC20M_FMT_UXGA,
		.pclk = 72000000,
		.linelength = 1600,
		.framelength = 1200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 30,
	},

	.margin = 0,					/* sensor framelength & shutter margin */
	.min_shutter = 12,				/* min shutter */
	.max_frame_length = 1220,		/* max framelength by sensor register's limitation */
	/* shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2 */
	.ae_shut_delay_frame = 0,
	/* sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2 */
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,				/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,			/* 1,le first ; 0, se first */
	.sensor_mode_num = 3,			/* support sensor mode num */	//Preview, capture, video

	.cap_delay_frame = 3,			/* enter capture delay frame num */
	.pre_delay_frame = 3,			/* enter preview delay frame num */
	.video_delay_frame = 3,			/* enter video delay frame num */

	.isp_driving_current = ISP_DRIVING_6MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,	/* sensor_interface_type */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_YUYV,	/* sensor output first pixel color */
	.mclk = 26,		/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_1_LANE,	/* mipi lane num */
	.i2c_addr = SR200PC20M_I2C_ADDR,	/* record sensor support all write id addr, only supprt 4must end with 0xff */
};

static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,	/* IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 30,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,	/* auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker */
	.test_pattern = KAL_FALSE,	/* test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = 0,		/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = SR200PC20M_I2C_ADDR,	/* record current sensor's i2c write id */
	.current_resolution = SR200PC20M_FMT_INVALID,
	.manual_wb_en = KAL_FALSE,	/* disable manual white balance control */
	.manual_exp_en = KAL_FALSE,	/* disable manual expsure control */
	.fixed_fps = KAL_FALSE,		/* use variable framerate for preview mode */
};

/*******************************************************************************
 * Utilities
 ********************************************************************************/
static int sensor_write_reg(u8 addr, u8 data)
{
	int ret = 0;
	u8 buf[2] = { addr, data };

	ret = iWriteRegI2C(buf, 2, SR200PC20M_WRITE_ID);

	//Check return value

	return ret;
}

static kal_uint8 sensor_read_reg(u8 addr)
{
	int ret = 0;
	u8 buf[2] = { 0x00, 0xFF };

	/* Read reg value */
	buf[0] = addr;
	ret = iReadRegI2C(&buf[0], 1, &buf[1], 1, SR200PC20M_WRITE_ID);

	//Check return value

	return buf[1];
}

static kal_uint8 sensor_read_id(void)
{
	sensor_write_reg(SR200PC20M_REG_PAGEMODE, REG_DEVID.addr);
	return sensor_read_reg(REG_DEVID.data);
}

static void sensor_select_page(u8 page)
{
	sensor_write_reg(SR200PC20M_REG_PAGEMODE, page);
}

/*******************************************************************************
 * 
 ********************************************************************************/
#define Sleep(ms) mdelay(ms)

MSDK_SENSOR_CONFIG_STRUCT SR200PC20M_SensorConfigData;
static DEFINE_SPINLOCK(imgsensor_drv_lock);

/*************************************************************************
* FUNCTION
*	SR200PC20M_SensorInit
*
* DESCRIPTION
*	This function apply all of the initial setting to sensor.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
*************************************************************************/
void SR200PC20M_SensorInit(void)
{
	int i = 0;

	SENSORDB("[%s]\n", __func__);

	for(i = 0; i < ARRAY_SIZE(sr200pc20m_seq_init_tbl); i++)
	{
		int ret = 0;

		ret = sensor_write_reg(sr200pc20m_seq_init_tbl[i].addr, sr200pc20m_seq_init_tbl[i].data);
#ifdef SENSOR_SETTING_DEBUG
		SENSORDB("[ini][%d] W 0x%02x>0x%02x [%d]\n", 
			i, 
			sr200pc20m_seq_init_tbl[i].data, 
			sr200pc20m_seq_init_tbl[i].addr, 
			ret);
#endif	
		//mdelay(sr200pc20m_seq_init_tbl[i].delay);

	}
}

/*************************************************************************
* FUNCTION
*	SR200PC20M_GetSensorID
*
* DESCRIPTION
*	This function apply all of the initial setting to sensor.
*
* PARAMETERS
*
* RETURNS
*
*************************************************************************/
UINT32 SR200PC20M_GetSensorID
(
	UINT32 *sensorID
)
{
	int retry = 3;

	SENSORDB("[%s]\n", __func__);
	
	do {
		*sensorID = (UINT32)sensor_read_id();
		if(*sensorID == (UINT32)SR200PC20M_SENSOR_ID) break;

		SENSORDB("Read sensor ID fail = 0x%02x\n", *sensorID);
		retry--;
	} while (retry > 0);

	if(*sensorID != SR200PC20M_SENSOR_ID)
	{
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	SR200PC20M_WriteMoreRegisters
*
* DESCRIPTION
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SR200PC20M_WriteMoreRegisters(void)
{
	/* Do nothing */
}

/*************************************************************************
 * FUNCTION
 *	SR200PC20M_Open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SR200PC20M_Open(void)
{
	signed char i;
	kal_uint16 sensor_id = 0;

	SENSORDB("[%s]\n", __func__);

	Sleep(10);

	/* Read sensor ID to adjust I2C is OK? */
	for (i = 0; i < 3; i++)
	{
		sensor_id = (kal_uint16)sensor_read_id();
		if (sensor_id != SR200PC20M_SENSOR_ID)
		{
			SENSORDB("Read Sensor ID Fail[open] = 0x%x\n", sensor_id);
			return ERROR_SENSOR_CONNECT_FAIL;
		}
	}
	
	SENSORDB("Sensor Read ID OK 0x%x\r\n", sensor_id);

	SR200PC20M_SensorInit();
	SR200PC20M_WriteMoreRegisters();
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	//imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.current_resolution = imgsensor_info.pre.resolution;
	spin_unlock(&imgsensor_drv_lock);
	
	return ERROR_NONE;
} /* SR200PC20M_Open */

/*************************************************************************
 * FUNCTION
 *	SR200PC20M_Close
 *
 * DESCRIPTION
 *	This function is to turn off sensor module power.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SR200PC20M_Close(void)
{
	SENSORDB("[%s]\n", __func__);
	return ERROR_NONE;
} /* SR200PC20M_Close */

/*************************************************************************
 * FUNCTION
 * preview_setting
 *************************************************************************/
static void preview_setting
(
	SR200PC20M_RES resolution
)
{ 
	int i = 0;
	int len = 0;
	SR200PC20M_REG * pReg = NULL;

	SENSORDB("[%s]\n", __func__);

	//For variable fps
	if(KAL_FALSE == imgsensor.fixed_fps)
	{
		switch(resolution)
		{
			case SR200PC20M_FMT_XGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_xga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_xga);
				break;
			case SR200PC20M_FMT_UXGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_uxga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_uxga);
				break;
			case SR200PC20M_FMT_SVGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_svga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_svga);
				break;
			case SR200PC20M_FMT_HD:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_hd;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_hd);
				break;
			case SR200PC20M_FMT_QVGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_qvga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_qvga);
				break;
			case SR200PC20M_FMT_QCIF:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_qcif;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_qcif);
				break;
			case SR200PC20M_FMT_480P:
				//pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_480p;
				//len = ARRAY_SIZE(sr200pc20m_seq_preview_480p);
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_svga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_svga);
				break;
			case SR200PC20M_FMT_VGA:
			default:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_vga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_vga);
				break;
		}
	}
	//For fixed fps
	else
	{
		switch(resolution)
		{
			case SR200PC20M_FMT_XGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_xga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_xga);
				break;
			case SR200PC20M_FMT_UXGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_uxga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_uxga);
				break;
			case SR200PC20M_FMT_SVGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_svga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_svga);
				break;
			case SR200PC20M_FMT_HD:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_hd;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_hd);
				break;
			case SR200PC20M_FMT_QVGA:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_qvga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_qvga);
				break;
			case SR200PC20M_FMT_QCIF:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_qcif;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_qcif);
				break;
			case SR200PC20M_FMT_480P:
				//pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_480p;
				//len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_480p);
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_svga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_svga);
				break;
			case SR200PC20M_FMT_VGA:
			default:
				pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_fixed_vga;
				len = ARRAY_SIZE(sr200pc20m_seq_preview_fixed_vga);
				break;
		}
	}

	if((pReg != NULL) && (len > 0))
	{
		for(i = 0; i < len; i++)
		{
			int ret = 0;
			ret = sensor_write_reg(pReg[i].addr, pReg[i].data);
#ifdef SENSOR_SETTING_DEBUG
		SENSORDB("[pre][%d] W 0x%02x>0x%02x [%d]\n",
			i,
			pReg[i].data,
			pReg[i].addr,
			ret);
#endif
		}
	}
	else
	{

	}
}

/*************************************************************************
 * FUNCTION
 * capture_setting
 *************************************************************************/
static void capture_setting
(
	SR200PC20M_RES resolution
)
{
	int i = 0;
	int len = 0;
	SR200PC20M_REG * pReg = NULL;

	SENSORDB("[%s]\n", __func__);
	switch(resolution)
	{
		case SR200PC20M_FMT_XGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_xga;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_xga);
			break;
		case SR200PC20M_FMT_UXGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_uxga;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_uxga);
			break;
		case SR200PC20M_FMT_SVGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_svga;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_svga);
			break;
		case SR200PC20M_FMT_HD:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_hd;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_hd);
			break;
		case SR200PC20M_FMT_QVGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_qvga;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_qvga);
			break;
		case SR200PC20M_FMT_QCIF:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_qcif;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_qcif);
			break;
		case SR200PC20M_FMT_480P:
			//pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_480p;
			//len = ARRAY_SIZE(sr200pc20m_seq_capture_480p);
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_svga;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_svga);
			break;
		case SR200PC20M_FMT_VGA:
		default:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_capture_vga;
			len = ARRAY_SIZE(sr200pc20m_seq_capture_vga);
			break;
	}

	if((pReg != NULL) && (len > 0))
	{
		for(i = 0; i < len; i++)
		{
			int ret = 0;
			ret = sensor_write_reg(pReg[i].addr, pReg[i].data);
#ifdef SENSOR_SETTING_DEBUG
		SENSORDB("[cap][%d] W 0x%02x>0x%02x [%d]\n",
			i,
			pReg[i].data,
			pReg[i].addr,
			ret);
#endif
		}
	}
	else
	{

	}
}

/*************************************************************************
 * FUNCTION
 * normal_video_setting
 *************************************************************************/
static void normal_video_setting
(
	SR200PC20M_RES resolution
)
{
	int i = 0;
	int len = 0;
	SR200PC20M_REG * pReg = NULL;

	SENSORDB("[%s]\n", __func__);
	switch(resolution)
	{
		case SR200PC20M_FMT_XGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_xga;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_xga);
			break;
		case SR200PC20M_FMT_UXGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_uxga;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_uxga);
			break;
		case SR200PC20M_FMT_SVGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_svga;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_svga);
			break;
		case SR200PC20M_FMT_HD:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_hd;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_hd);
			break;
		case SR200PC20M_FMT_QVGA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_qvga;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_qvga);
			break;
		case SR200PC20M_FMT_QCIF:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_qcif;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_qcif);
			break;
		case SR200PC20M_FMT_480P:
			//pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_480p;
			//len = ARRAY_SIZE(sr200pc20m_seq_preview_480p);
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_svga;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_svga);
			break;
		case SR200PC20M_FMT_VGA:
		default:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_preview_vga;
			len = ARRAY_SIZE(sr200pc20m_seq_preview_vga);
			break;
	}

	if((pReg != NULL) && (len > 0))
	{
		for(i = 0; i < len; i++)
		{
			int ret = 0;
			ret = sensor_write_reg(pReg[i].addr, pReg[i].data);
#ifdef SENSOR_SETTING_DEBUG
		SENSORDB("[normal_vid][%d] W 0x%02x>0x%02x [%d]\n",
			i,
			pReg[i].data,
			pReg[i].addr,
			ret);
#endif
		}
	}
	else
	{

	}
}
/*************************************************************************
 * FUNCTION
 * SR200PC20M_Preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SR200PC20M_Preview
(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT          *sensor_config_data
)
{
	SENSORDB("[%s]\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.current_resolution = imgsensor_info.pre.resolution;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting(imgsensor.current_resolution);

	memcpy(&SR200PC20M_SensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

	return ERROR_NONE;
} /* SR200PC20M_Preview */

/*************************************************************************
 * FUNCTION
 *	SR200PC20M_Capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SR200PC20M_Capture
(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT          *sensor_config_data
)
{
	SENSORDB("[%s]\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.current_resolution = imgsensor_info.cap.resolution;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_resolution);

	memcpy(&SR200PC20M_SensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

	return ERROR_NONE;
} /* SR200PC20M_Capture() */

/*************************************************************************
 * FUNCTION
 *	SR200PC20M_NormalVideo
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in normal video mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SR200PC20M_NormalVideo
(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT          *sensor_config_data
)
{
	SENSORDB("[%s]\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.current_resolution = imgsensor_info.normal_video.resolution;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_resolution);

	memcpy(&SR200PC20M_SensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

	return ERROR_NONE;
} /* SR200PC20M_Capture() */

/*************************************************************************
 * FUNCTION
 * SR200PC20M_GetResolution
 *************************************************************************/
UINT32 SR200PC20M_GetResolution
(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution
)
{
	SENSORDB("[%s]\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	pSensorResolution->SensorFullWidth		= imgsensor_info.cap.grabwindow_width;
	pSensorResolution->SensorFullHeight		= imgsensor_info.cap.grabwindow_height;
	pSensorResolution->SensorPreviewWidth	= imgsensor_info.pre.grabwindow_width;
	pSensorResolution->SensorPreviewHeight	= imgsensor_info.pre.grabwindow_height;
	pSensorResolution->SensorVideoWidth		= imgsensor_info.normal_video.grabwindow_width;
	pSensorResolution->SensorVideoHeight	= imgsensor_info.normal_video.grabwindow_height;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
} /* SR200PC20M_GetResolution() */

/*************************************************************************
 * FUNCTION
 * SR200PC20M_GetInfo
 *************************************************************************/
UINT32 SR200PC20M_GetInfo
(
	MSDK_SCENARIO_ID_ENUM      ScenarioId,
	MSDK_SENSOR_INFO_STRUCT   *pSensorInfo,
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData
)
{
	SENSORDB("[%s] ScenarioId:0x%x\n", __func__, ScenarioId);

	spin_lock(&imgsensor_drv_lock);
	pSensorInfo->SensorClockPolarity			= SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity		= SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	pSensorInfo->SensorHsyncPolarity			= SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	pSensorInfo->SensorVsyncPolarity			= SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorInterruptDelayLines		= 4;	/* not use */
	pSensorInfo->SensorResetActiveHigh			= FALSE;	/* not use */
	pSensorInfo->SensorResetDelayCount			= 5;	/* not use */

	pSensorInfo->SensroInterfaceType			= imgsensor_info.sensor_interface_type;
	pSensorInfo->MIPIsensorType					= imgsensor_info.mipi_sensor_type;
	pSensorInfo->SettleDelayMode				= imgsensor_info.mipi_settle_delay_mode;
	pSensorInfo->SensorOutputDataFormat			= imgsensor_info.sensor_output_dataformat;

	pSensorInfo->CaptureDelayFrame				= imgsensor_info.cap_delay_frame;
	pSensorInfo->PreviewDelayFrame				= imgsensor_info.pre_delay_frame;
	pSensorInfo->VideoDelayFrame				= imgsensor_info.video_delay_frame;

	pSensorInfo->SensorMasterClockSwitch		= 0;	/* not use */
	pSensorInfo->SensorDrivingCurrent			= imgsensor_info.isp_driving_current;
	/* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AEShutDelayFrame				= imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	pSensorInfo->AESensorGainDelayFrame			= imgsensor_info.ae_sensor_gain_delay_frame;
	pSensorInfo->AEISPGainDelayFrame			= imgsensor_info.ae_ispGain_delay_frame;
	pSensorInfo->IHDR_Support					= imgsensor_info.ihdr_support;
	pSensorInfo->IHDR_LE_FirstLine				= imgsensor_info.ihdr_le_firstline;
	pSensorInfo->SensorModeNum					= imgsensor_info.sensor_mode_num;

	pSensorInfo->SensorMIPILaneNumber			= imgsensor_info.mipi_lane_num;
	pSensorInfo->SensorClockFreq				= imgsensor_info.mclk;
	pSensorInfo->SensorClockDividCount			= 3;	/* not use */
	pSensorInfo->SensorClockRisingCount			= 0;
	pSensorInfo->SensorClockFallingCount		= 2;	/* not use */
	pSensorInfo->SensorPixelClockCount			= 3;	/* not use */
	pSensorInfo->SensorDataLatchCount			= 2;	/* not use */

	pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount		= 0;
	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount		= 0;
	pSensorInfo->SensorWidthSampling						= 0;	/* 0 is default 1x */
	pSensorInfo->SensorHightSampling						= 0;	/* 0 is default 1x */
	pSensorInfo->SensorPacketECCOrder						= 1;

	switch (ScenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			pSensorInfo->SensorGrabStartX = imgsensor_info.cap.startx;
			pSensorInfo->SensorGrabStartY = imgsensor_info.cap.starty;
			pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorGrabStartX = imgsensor_info.normal_video.startx;
			pSensorInfo->SensorGrabStartY = imgsensor_info.normal_video.starty;
			pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			pSensorInfo->SensorGrabStartX = imgsensor_info.pre.startx;
			pSensorInfo->SensorGrabStartY = imgsensor_info.pre.starty;
			pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	memcpy(pSensorConfigData, &SR200PC20M_SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
} /* SR200PC20M_GetInfo() */

/*************************************************************************
 * FUNCTION
 * SR200PC20M_Control
 *************************************************************************/
UINT32 SR200PC20M_Control
(
	MSDK_SCENARIO_ID_ENUM               ScenarioId,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
	MSDK_SENSOR_CONFIG_STRUCT          *pSensorConfigData
)
{
#ifdef SENSOR_SETTING_DEBUG
	SENSORDB("[%s] %d\n", __func__, ScenarioId);
	SENSORDB("[%s] SensorOperationMode:%u\n", __func__, (unsigned int)(pSensorConfigData->SensorOperationMode));
	SENSORDB("[%s] SensorImageMirror  :%u\n", __func__, (unsigned int)(pSensorConfigData->SensorImageMirror));
	SENSORDB("[%s] ImageTargetWidth   :%u\n", __func__, (unsigned int)(pSensorConfigData->ImageTargetWidth));
	SENSORDB("[%s] ImageTargetHeight  :%u\n", __func__, (unsigned int)(pSensorConfigData->ImageTargetHeight));
	SENSORDB("[%s] DefaultPclk        :%u\n", __func__, pSensorConfigData->DefaultPclk);
	SENSORDB("[%s] Pixels             :%u\n", __func__, pSensorConfigData->Pixels);
	SENSORDB("[%s] Lines              :%u\n", __func__, pSensorConfigData->Lines);
	SENSORDB("[%s] FrameLines         :%u\n", __func__, pSensorConfigData->FrameLines);
#endif
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = ScenarioId;
	spin_unlock(&imgsensor_drv_lock);

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		SR200PC20M_Capture(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		SR200PC20M_NormalVideo(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	default:
		SR200PC20M_Preview(pImageWindow, pSensorConfigData);
		break;
	}

	return ERROR_NONE;
} /* SR200PC20M_Control() */

/*************************************************************************
 * Image properties: Saturation
 *************************************************************************/
static void SR200PC20M_YUVSetSaturation(UINT16 param)
{
	switch((ISP_SAT_T)param)
	{
		case ISP_SAT_MIDDLE:
			break;
		case ISP_SAT_HIGH:
			break;
		case ISP_SAT_LOW:
			break;
		default:
			break;
	}
}

/*************************************************************************
 * Image properties: Brightness
 *************************************************************************/
static void SR200PC20M_YUVSetBrightness(UINT16 param)
{
	switch((ISP_BRIGHT_T)param)
	{
		case ISP_BRIGHT_MIDDLE:
			break;
		case ISP_BRIGHT_HIGH:
			break;
		case ISP_BRIGHT_LOW:
			break;
		default:
			break;
	}
}

/*************************************************************************
 * Image properties: Contrast
 *************************************************************************/
static void SR200PC20M_YUVSetContrast(UINT16 param)
{
	switch((ISP_CONTRAST_T)param)
	{
		case ISP_CONTRAST_MIDDLE:
			break;
		case ISP_CONTRAST_HIGH:
			break;
		case ISP_CONTRAST_LOW:
			break;
		default:
			break;
	}
}

/*************************************************************************
 * Binary effect
 *************************************************************************/
#ifdef SENSOR_BINARY_EFFECT_SUPPORT
static void binary_effect_enable(kal_bool en)
{
	kal_uint8 uVal = 0;

	sensor_select_page(PAGE10);
	uVal = sensor_read_reg(0x13);
	uVal &= ~(1<<0);

	if(KAL_TRUE == en)
		uVal |= (1<<0);

	sensor_write_reg(0x13, uVal);
}

static void binary_effect_set_threshold(kal_uint8 threshold)
{
	sensor_select_page(PAGE10);
	sensor_write_reg(0x47, threshold);
}
#endif
/*************************************************************************
 * Mirror (flip) control
 *************************************************************************/
 #ifdef SENSOR_MIRROR_EFFECT_SUPPORT
static void mirror_control(kal_uint8 type)
{
	kal_uint8 uVal = 0;

	sensor_select_page(PAGE0);

	uVal = sensor_read_reg(0x11);
	uVal &= ~(3<<0);

	switch(type)
	{
		case IMAGE_H_MIRROR:
			uVal |= (1<<0);
			break;
		case IMAGE_V_MIRROR:
			uVal |= (1<<1);
			break;
		case IMAGE_HV_MIRROR:
			uVal |= (3<<0);
			break;
		case IMAGE_NORMAL:
		default:
			break;
	}

	sensor_write_reg(0x11, uVal);
	imgsensor.mirror = type;
}
#endif
/*************************************************************************
 * White balance
 *************************************************************************/
static void SR200PC20M_YUVSetWhiteBalance(UINT16 param)
{
	int i = 0;
	int len = 0;
	SR200PC20M_REG * pReg = NULL;

	//sensor_select_page(PAGE22);
	imgsensor.manual_wb_en = KAL_TRUE;

	switch((AWB_MODE_T)param)
	{
		case AWB_MODE_INCANDESCENT:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_wb_incandescent;
			len = ARRAY_SIZE(sr200pc20m_seq_wb_incandescent);
			break;
		case AWB_MODE_DAYLIGHT:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_wb_daylight;
			len = ARRAY_SIZE(sr200pc20m_seq_wb_daylight);
			break;
		case AWB_MODE_FLUORESCENT:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_wb_flourescent;
			len = ARRAY_SIZE(sr200pc20m_seq_wb_flourescent);
			break;
		case AWB_MODE_CLOUDY_DAYLIGHT:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_wb_cloudy;
			len = ARRAY_SIZE(sr200pc20m_seq_wb_cloudy);
			break;
		case AWB_MODE_AUTO:
		case AWB_MODE_OFF:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_wb_auto;
			len = ARRAY_SIZE(sr200pc20m_seq_wb_auto);
			imgsensor.manual_wb_en = KAL_FALSE;
			break;
		default:
			break;
	}

	if((pReg != NULL) && (len > 0))
	{
		for(i = 0; i < len; i++)
		{
			int ret = 0;
			ret = sensor_write_reg(pReg[i].addr, pReg[i].data);
		}
	}
}

/*************************************************************************
 * Color Effects
 *************************************************************************/
static void SR200PC20M_YUVSetColorEffect(UINT16 param)
{
	int i = 0;
	int len = 0;
	SR200PC20M_REG * pReg = NULL;

	switch((MCOLOR_EFFECT)param)
	{
		case MEFFECT_MONO:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_effect_mono;
			len = ARRAY_SIZE(sr200pc20m_seq_effect_mono);
			break;
		case MEFFECT_SEPIA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_effect_sepia;
			len = ARRAY_SIZE(sr200pc20m_seq_effect_sepia);
			break;
		case MEFFECT_AQUA:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_effect_aqua;
			len = ARRAY_SIZE(sr200pc20m_seq_effect_aqua);
			break;		
		case MEFFECT_OFF:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_effect_normal;
			len = ARRAY_SIZE(sr200pc20m_seq_effect_normal);
			break;	
		default:
			break;
	}

	if((pReg != NULL) && (len > 0))
	{
		for(i = 0; i < len; i++)
		{
			int ret = 0;
			ret = sensor_write_reg(pReg[i].addr, pReg[i].data);
		}
	}
}

/*************************************************************************
 * Exposure
 *************************************************************************/
static void SR200PC20M_YUVSetExposure(UINT16 param)
{
	switch((AE_EVCOMP_T)param)
	{
		case AE_EV_COMP_20:
			break;
		case AE_EV_COMP_10:
			break;
		case AE_EV_COMP_00:
			break;
		case AE_EV_COMP_n10:
			break;
		case AE_EV_COMP_n20:
			break;
		default:
			break;
	}
}

static kal_uint32 SR200PC20M_read_shutter(void)
{
	kal_uint8 temp_reg0, temp_reg1, temp_reg2; 
	kal_uint32 shutter; 

	sensor_select_page(PAGE20);
	temp_reg0 = sensor_read_reg(0x80); 
	temp_reg1 = sensor_read_reg(0x81); 
	temp_reg2 = sensor_read_reg(0x82); 
	shutter = (temp_reg0 << 16) | (temp_reg1 << 8) | (temp_reg2 & 0xFF); 

	return shutter; 
}

/*************************************************************************
 * Anti-Flicker
 *************************************************************************/
static void SR200PC20M_YUVSetAntiFlicker(UINT16 param)
{
	switch((AE_FLICKER_MODE_T)param)
	{
		case AE_FLICKER_MODE_OFF:
			break;
		case AE_FLICKER_MODE_AUTO:
			break;
		case AE_FLICKER_MODE_50HZ:
			break;
		case AE_FLICKER_MODE_60HZ:
			break;
		default:
			break;
	}
}

/*************************************************************************
 * ISO
 *************************************************************************/
static void SR200PC20M_YUVSetISO(UINT16 param)
{
	switch((AE_ISO_T)param)
	{
		case AE_ISO_AUTO:
			break;
		case AE_ISO_100:
			break;
		case AE_ISO_200:
			break;
		case AE_ISO_400:
			break;
		case AE_ISO_800:
			break;
		case AE_ISO_1600:
			break;
		default:
			break;
	}
}

/*************************************************************************
 * Sharpness (Edge enhancement)
 *************************************************************************/
static void SR200PC20M_YUVSetSharpness(UINT16 param)
{
	switch((ISP_EDGE_T)param)
	{
		case ISP_EDGE_LOW:
			break;
		case ISP_EDGE_MIDDLE:
			break;
		case ISP_EDGE_HIGH:
			break;
		default:
			break;
	}
}

/*************************************************************************
 * Scene mode
 *************************************************************************/
static void SR200PC20M_YUVSetScene(UINT16 param)
{
	int i = 0;
	int len = 0;
	SR200PC20M_REG * pReg = NULL;

	switch((SCENE_MODE_T)param)
	{
		case SCENE_MODE_OFF:
		case SCENE_MODE_NORMAL:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_auto;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_auto);
			break;
		case SCENE_MODE_SPORTS:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_sports;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_sports);
			break;
		case SCENE_MODE_PORTRAIT:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_portrait;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_portrait);
			break;
		case SCENE_MODE_NIGHTSCENE:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_night;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_night);
			break;
		case SCENE_MODE_THEATRE:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_theater;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_theater);
			break;
		case SCENE_MODE_BEACH:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_beach;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_beach);
			break;
		case SCENE_MODE_SNOW:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_snow;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_snow);
			break;
		case SCENE_MODE_SUNSET:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_sunset;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_sunset);
			break;
		case SCENE_MODE_FIREWORKS:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_fireworks;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_fireworks);
			break;
		case SCENE_MODE_PARTY:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_party;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_party);
			break;
		case SCENE_MODE_CANDLELIGHT:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_candlelight;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_candlelight);
			break;
		case SCENE_MODE_NIGHTPORTRAIT:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_nightsnap;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_nightsnap);
			break;
		case SCENE_MODE_LANDSCAPE:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_scenery;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_scenery);
			break;
		case SCENE_MODE_STEADYPHOTO:
			pReg = (SR200PC20M_REG *)sr200pc20m_seq_scene_comphand;
			len = ARRAY_SIZE(sr200pc20m_seq_scene_comphand);
			break;
		default:
			break;
	}

	if((pReg != NULL) && (len > 0))
	{
		for(i = 0; i < len; i++)
		{
			int ret = 0;
			ret = sensor_write_reg(pReg[i].addr, pReg[i].data);
#ifdef SENSOR_SETTING_DEBUG
		SENSORDB("[sce][%d][%d] W 0x%02x>0x%02x [%d]\n",
			param,
			i,
			pReg[i].data,
			pReg[i].addr,
			ret);
#endif
		}
	}
}

static UINT32 SR200PC20M_YUVSensorSetting(FEATURE_ID iCmd, UINT16 iPara)
{
	SENSORDB("[%s] iCmd :%d iPara:%d\n", __func__, iCmd, iPara);

	switch(iCmd)
	{
		case FID_COLOR_EFFECT:
			SR200PC20M_YUVSetColorEffect(iPara);
			break;
		case FID_AWB_MODE:
			SR200PC20M_YUVSetWhiteBalance(iPara);
			break;
		case FID_AE_EV:
			SR200PC20M_YUVSetExposure(iPara);
			break;
		case FID_AE_FLICKER:
			SR200PC20M_YUVSetAntiFlicker(iPara);
			break;
		case FID_ISP_BRIGHT:
			SR200PC20M_YUVSetBrightness(iPara);
			break;
		case FID_ISP_CONTRAST:
			SR200PC20M_YUVSetContrast(iPara);
			break;
		case FID_ISP_SAT:
			SR200PC20M_YUVSetSaturation(iPara);
			break;
		case FID_AE_ISO:
			SR200PC20M_YUVSetISO(iPara);
			break;
		case FID_ISP_EDGE:
			SR200PC20M_YUVSetSharpness(iPara);
			break;
		//case FID_ZOOM_FACTOR:
		//case FID_EIS:					//Not implemented on HAL
		//case FID_ZSD:					//Not implemented on HAL
		//case FID_PREVIEW_SIZE:		//Not implemented on HAL
		//case FID_VIDEO_PREVIEW_SIZE:	//Not implemented on HAL
		case FID_SCENE_MODE:
			SR200PC20M_YUVSetScene(iPara);
			break;
		case FID_ISP_HUE:
		default:
			break;
	}
	return KAL_TRUE;
}

static UINT32 SR200PC20M_YUVSetVideoMode(UINT16 u2FrameRate)
{
	SENSORDB("[%s] u2FrameRate:%d\n", __func__, u2FrameRate);
	return 0;
}

static void SR200PC20M_YUV_GetExifInfo(UINT32 exifAddr) 
{ 
  SENSOR_EXIF_INFO_STRUCT* pExifInfo = (SENSOR_EXIF_INFO_STRUCT*)((MUINT32 *)(uintptr_t)(exifAddr)); 

//	pExifInfo->FNumber = 28; 
//	pExifInfo->AEISOSpeed = AE_ISO_100; 
//	pExifInfo->AWBMode = ;
	pExifInfo->CapExposureTime = SR200PC20M_read_shutter(); 
//	pExifInfo->FlashLightTimeus = 0; 
//	pExifInfo->RealISOValue = AE_ISO_100; 
} 

static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate) 
{
	SENSORDB("[%s] scenario_id = %d\n", __func__, scenario_id);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate) 
{
	//kal_uint32 frame_length;

	SENSORDB("[%s] scenario_id = %d, framerate = %d\n", __func__, scenario_id, framerate);
#if 0
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			break;
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}
#endif
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 * SR200PC20M_FeatureControl
 *************************************************************************/
UINT32 SR200PC20M_FeatureControl
(
	MSDK_SENSOR_FEATURE_ENUM FeatureId,
	UINT8                   *pFeaturePara,
	UINT32                  *pFeatureParaLen
)
{
	UINT16 *pFeatureReturnPara16                 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16                       = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32                 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32                       = (UINT32 *) pFeaturePara;
	unsigned long long *pFeatureData             = (unsigned long long *)pFeaturePara;
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData = (MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
//	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData = (MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

	SENSORDB("[%s] FeatureId: 0x%x\n", __func__, FeatureId);

	switch (FeatureId) {
	case SENSOR_FEATURE_GET_RESOLUTION:
		SENSORDB("SR200PC20M_FeatureControl SENSOR_FEATURE_GET_RESOLUTION\n");
//		*pFeatureReturnPara16++ = IMAGE_SENSOR_FULL_WIDTH;
//		*pFeatureReturnPara16 = IMAGE_SENSOR_FULL_HEIGHT;
//		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		SENSORDB("SR200PC20M_FeatureControl SENSOR_FEATURE_GET_PERIOD\n");
		*pFeatureReturnPara16++ = imgsensor.line_length;
		*pFeatureReturnPara16 = imgsensor.frame_length;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*pFeatureReturnPara32 = imgsensor_info.pre.pclk;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		break;
	case SENSOR_FEATURE_SET_GAIN:
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		//SR200PC20M_isp_master_clock = *pFeatureData32;
		break;
	case SENSOR_FEATURE_SET_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER:
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		memcpy(pSensorConfigData, &SR200PC20M_SensorConfigData,
		       sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		*pFeatureParaLen = sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_GET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_GET_GROUP_COUNT:
	case SENSOR_FEATURE_GET_GROUP_INFO:
	case SENSOR_FEATURE_SET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		SR200PC20M_YUVSetVideoMode(*pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_YUV_CMD:
		SR200PC20M_YUVSensorSetting((FEATURE_ID) *pFeatureData, *(pFeatureData + 1));
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		SR200PC20M_GetSensorID(pFeatureData32);
		SENSORDB("Sensor ID: 0x%x\n", *pFeatureData32);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*pFeatureReturnPara32 = SR200PC20M_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData, (MUINT32 *)(uintptr_t)(*(pFeatureData+1)));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData, *(pFeatureData+1));
		break;
	case SENSOR_FEATURE_GET_EXIF_INFO:   
		SR200PC20M_YUV_GetExifInfo(*pFeatureData32);   
		break; 
	default:
		break;
	}
	return ERROR_NONE;
} /* SR200PC20M_FeatureControl() */

/*************************************************************************
 * 
 *************************************************************************/
SENSOR_FUNCTION_STRUCT SensorFuncSR200PC20M_MIPI_YUV = {
	SR200PC20M_Open,
	SR200PC20M_GetInfo,
	SR200PC20M_GetResolution,
	SR200PC20M_FeatureControl,
	SR200PC20M_Control,
	SR200PC20M_Close
};

/*************************************************************************
 * 
 *************************************************************************/
UINT32 SR200PC20M_MIPI_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	SENSORDB("[%s]\n", __func__);
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncSR200PC20M_MIPI_YUV;
	return ERROR_NONE;
} /* SensorInit() */
