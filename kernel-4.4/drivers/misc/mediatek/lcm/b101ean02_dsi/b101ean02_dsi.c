/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* Copyright (C) 2019 SANYO Techno Solutions Tottori Co., Ltd.
*
*/

#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif

#include "lcm_drv.h"

static struct regulator *lcm_vgp;

static unsigned int GPIO_LCD_STBY;
static unsigned int GPIO_LCD_BL_EN;
static unsigned int GPIO_LCD_RST;

static int lcm_get_gpio(struct device *dev)
{
	GPIO_LCD_STBY = of_get_named_gpio(dev->of_node, "gpio_lcd_stby", 0);
	gpio_request(GPIO_LCD_STBY, "GPIO_LCD_STBY");
	pr_err("[Kernel/LCM] GPIO_LCD_STBY = 0x%x\n", GPIO_LCD_STBY);

	GPIO_LCD_BL_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_bl_en", 0);
	gpio_request(GPIO_LCD_BL_EN, "GPIO_LCD_BL_EN");
	pr_err("[Kernel/LCM] GPIO_LCD_BL_EN = 0x%x\n", GPIO_LCD_BL_EN);

	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	pr_err("[Kernel/LCM] GPIO_LCD_RST = 0x%x\n", GPIO_LCD_RST);

	return 0;
}

/* get LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
#if 1
	int ret;
	struct regulator *lcm_vgp_ldo;

	pr_err("Kernel/LCM: lcm_get_vgp_supply is going\n");

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		dev_err(dev, "failed to get reg-lcm LDO, %d\n", ret);
		return ret;
	}

	pr_err("Kernel/LCM: lcm get supply ok.\n");

	ret = regulator_enable(lcm_vgp_ldo);
	/* get current voltage settings */
	ret = regulator_get_voltage(lcm_vgp_ldo);
	pr_err("Kernel/lcm LDO voltage = %d in LK stage\n", ret);

	lcm_vgp = lcm_vgp_ldo;

	return ret;
#else
	//LDO voltage control GPIO
	return 0;
#endif
}

static int lcm_vgp_supply_enable(void)
{
#if 1
	int ret;
	unsigned int volt;

	pr_err("Kernel/LCM: lcm_vgp_supply_enable\n");

	if (NULL == lcm_vgp)
		return 0;

	pr_err("Kernel/LCM: set regulator voltage lcm_vgp voltage to 1.8V\n");
	/* set voltage to 1.8V */
	ret = regulator_set_voltage(lcm_vgp, 1800000, 1800000);
	if (ret != 0) {
		pr_err("Kernel/LCM: lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 1800000)
		pr_err("Kernel/LCM: check regulator voltage=1800000 pass!\n");
	else
		pr_err("Kernel/LCM: check regulator voltage=1800000 fail! (voltage: %d)\n", volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		pr_err("Kernel/LCM: Failed to enable lcm_vgp: %d\n", ret);
		return ret;
	}

	return ret;
#else
	return 0;
#endif
}

static int lcm_vgp_supply_disable(void)
{
#if 1
	int ret = 0;
	unsigned int isenable;

	if (NULL == lcm_vgp)
		return 0;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	pr_err("Kernel/LCM: lcm query regulator enable status[0x%d]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			pr_err("Kernel/LCM: lcm failed to disable lcm_vgp: %d\n", ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			pr_err("Kernel/LCM: lcm regulator disable pass\n");
	}

	return ret;
#else
	return 0;
#endif
}

static int lcm_gpio_probe(struct platform_device *pdev)
{
	struct device	*dev = &pdev->dev;
	pr_err("Kernel/LCM: %s start \n",__func__);

	lcm_get_vgp_supply(dev);
	lcm_get_gpio(dev);

	return 0;
}


#if 0
static int lcm_probe(struct device *dev)
{
	pr_err("Kernel/LCM: %s start \n",__func__);
	lcm_get_vgp_supply(dev);
	lcm_get_gpio(dev);

	return 0;
}
#endif
static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "mediatek,b101ean02_dsi",},
	{}
};

static struct platform_driver lcm_driver = {
    .probe = lcm_gpio_probe,
	.driver = {
		   .name = "b101ean02_dsi",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init lcm_init(void)
{
	pr_notice("LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_err("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}
late_initcall(lcm_init);
module_exit(lcm_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)

#ifndef	GPIO_OUT_ZERO
#define	GPIO_OUT_ZERO	(0)
#endif

#ifndef	GPIO_OUT_ONE
#define	GPIO_OUT_ONE	(1)
#endif

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = {0};

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define MDELAY(n) (lcm_util.mdelay(n))
#define REGFLAG_DELAY             0xFE
#define REGFLAG_END_OF_TABLE      0xFF   // END OF REGISTERS MARKER

#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)

static struct LCM_setting_table lcm_initialization_setting[] = {
	//Page0
	{0xE0, 1, {0x00}},
	//--- PASSWORD  ----//
	{0xE1, 1, {0x93}},
	{0xE2, 1, {0x65}},
	{0xE3, 1, {0xF8}},
	{0x80, 1, {0x03}},

	{0xE0, 1, {0x04}},
	{0x2B, 1, {0x2B}},
	{0x2D, 1, {0x03}},
	{0x2E, 1, {0x44}},

	//Page1
	{0xE0, 1, {0x01}},

	//Set VCOM                                                                                          
	{0x00, 1, {0x00}},
	{0x01, 1, {0x6A}},                                                                         

	//Set Gamma Power, VGMP,VGMN,VGSP,VGSN
	{0x17, 1, {0x00}},
	{0x18, 1, {0xC0}},
	{0x19, 1, {0x01}},
	{0x1A, 1, {0x00}},
	{0x1B, 1, {0xC0}},
	{0x1C, 1, {0x01}},

	//Set Gate Power
	{0x1F, 1, {0x6A}},
	{0x20, 1, {0x23}},
	{0x21, 1, {0x23}},
	{0x22, 1, {0x0E}},

	{0x35, 1, {0x28}},

	//SET RGBCYC
	{0x37, 1, {0x59}},
	{0x38, 1, {0x05}},
	{0x39, 1, {0x04}},
	{0x3A, 1, {0x08}},
	{0x3B, 1, {0x08}},
	{0x3C, 1, {0x7C}},
	{0x3D, 1, {0xFF}},
	{0x3E, 1, {0xFF}},
	{0x3F, 1, {0xFF}},

	//Set TCON
	{0x40, 1, {0x06}},
	{0x41, 1, {0xA0}},

	{0x43, 1, {0x08}},
	{0x44, 1, {0x0B}},
	{0x45, 1, {0x88}},

//	{0x4A, 1, {0x35}},//for bist mode

	//--- power voltage  ----//
	{0x55, 1, {0x0F}},
	{0x56, 1, {0x01}},
	{0x57, 1, {0xAD}},
	{0x58, 1, {0x0A}},
	{0x59, 1, {0x0A}},
	{0x5A, 1, {0x28}},
	{0x5B, 1, {0x1E}},

	//--- Gamma 2.2 ----// 
	{0x5D, 1, {0x5C}},
	{0x5E, 1, {0x40}},
	{0x5F, 1, {0x30}},
	{0x60, 1, {0x24}},
	{0x61, 1, {0x21}},
	{0x62, 1, {0x13}},
	{0x63, 1, {0x1A}},
	{0x64, 1, {0x07}},
	{0x65, 1, {0x23}},
	{0x66, 1, {0x25}},
	{0x67, 1, {0x27}},
	{0x68, 1, {0x47}},
	{0x69, 1, {0x37}},
	{0x6A, 1, {0x3F}},
	{0x6B, 1, {0x32}},
	{0x6C, 1, {0x2E}},
	{0x6D, 1, {0x22}},
	{0x6E, 1, {0x11}},
	{0x6F, 1, {0x00}},
	{0x70, 1, {0x5C}},
	{0x71, 1, {0x40}},
	{0x72, 1, {0x30}},
	{0x73, 1, {0x24}},
	{0x74, 1, {0x21}},
	{0x75, 1, {0x13}},
	{0x76, 1, {0x1A}},
	{0x77, 1, {0x07}},
	{0x78, 1, {0x23}},
	{0x79, 1, {0x25}},
	{0x7A, 1, {0x27}},
	{0x7B, 1, {0x47}},
	{0x7C, 1, {0x37}},
	{0x7D, 1, {0x3F}},
	{0x7E, 1, {0x32}},
	{0x7F, 1, {0x2E}},
	{0x80, 1, {0x22}},
	{0x81, 1, {0x11}},
	{0x82, 1, {0x00}},

	//Page2, for GIP
	{0xE0, 1, {0x02}},

	//GIP_L Pin mapping
	{0x00, 1, {0x44}},
	{0x01, 1, {0x44}},
	{0x02, 1, {0x45}},
	{0x03, 1, {0x45}},
	{0x04, 1, {0x46}},
	{0x05, 1, {0x46}},
	{0x06, 1, {0x47}},
	{0x07, 1, {0x47}},
	{0x08, 1, {0x1D}},
	{0x09, 1, {0x1D}},
	{0x0A, 1, {0x1D}},
	{0x0B, 1, {0x1D}},
	{0x0C, 1, {0x1D}},
	{0x0D, 1, {0x1D}},
	{0x0E, 1, {0x1D}},
	{0x0F, 1, {0x57}},
	{0x10, 1, {0x57}},
	{0x11, 1, {0x58}},
	{0x12, 1, {0x58}},
	{0x13, 1, {0x40}},
	{0x14, 1, {0x55}},
	{0x15, 1, {0x55}},

	//GIP_R Pin mapping
	{0x16, 1, {0x44}},
	{0x17, 1, {0x44}},
	{0x18, 1, {0x45}},
	{0x19, 1, {0x45}},
	{0x1A, 1, {0x46}},
	{0x1B, 1, {0x46}},
	{0x1C, 1, {0x47}},
	{0x1D, 1, {0x47}},
	{0x1E, 1, {0x1D}},
	{0x1F, 1, {0x1D}},
	{0x20, 1, {0x1D}},
	{0x21, 1, {0x1D}},
	{0x22, 1, {0x1D}},
	{0x23, 1, {0x1D}},
	{0x24, 1, {0x1D}},
	{0x25, 1, {0x57}},
	{0x26, 1, {0x57}},
	{0x27, 1, {0x58}},
	{0x28, 1, {0x58}},
	{0x29, 1, {0x40}},
	{0x2A, 1, {0x55}},
	{0x2B, 1, {0x55}},

	//GIP Timing  
	{0x58, 1, {0x40}},
	{0x59, 1, {0x00}},
	{0x5A, 1, {0x00}},
	{0x5B, 1, {0x00}},
	{0x5C, 1, {0x0A}},
	{0x5D, 1, {0x10}},
	{0x5E, 1, {0x01}},
	{0x5F, 1, {0x02}},
	{0x60, 1, {0x10}},
	{0x61, 1, {0x01}},
	{0x62, 1, {0x02}},
	{0x63, 1, {0x0B}},
	{0x64, 1, {0x54}},
	{0x65, 1, {0x45}},
	{0x66, 1, {0x07}},
	{0x67, 1, {0x31}},
	{0x68, 1, {0x0B}},
	{0x69, 1, {0x1E}},
	{0x6A, 1, {0x54}},
	{0x6B, 1, {0x04}},
	{0x6C, 1, {0x00}},
	{0x6D, 1, {0x04}},
	{0x6E, 1, {0x00}},
	{0x6F, 1, {0x88}},
	{0x70, 1, {0x00}},
	{0x71, 1, {0x00}},
	{0x72, 1, {0x06}},
	{0x73, 1, {0x7B}},
	{0x74, 1, {0x00}},
	{0x75, 1, {0xF8}},
	{0x76, 1, {0x00}},
	{0x77, 1, {0x0D}},
	{0x78, 1, {0x14}},
	{0x79, 1, {0x00}},
	{0x7A, 1, {0x00}},
	{0x7B, 1, {0x00}},
	{0x7C, 1, {0x00}},
	{0x7D, 1, {0x03}},
	{0x7E, 1, {0x7B}},

	//Page0
	{0xE0, 1, {0x00}},
	{0xE6, 1, {0x02}},
	{0xE7, 1, {0x06}},

	//SLP OUT
	{0x11, 0, {0x00}},
	{REGFLAG_DELAY, 120, {0x00}},
	//DISP ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 50, {0x00}},

	//--- TE----//
	{0x35, 1, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {0x00}}
};

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_set_value(GPIO, output);
}

static void lcm_init_power(void)
{
	pr_err("[Kernel/LCM] lcm_init_power() enter\n");
}

static void lcm_suspend_power(void)
{
	pr_err("[Kernel/LCM] lcm_suspend_power() enter\n");
}

static void lcm_resume_power(void)
{
	pr_err("[Kernel/LCM] lcm_resume_power() enter\n");
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for(i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;
		switch (cmd) {
			case REGFLAG_DELAY :
				MDELAY(table[i].count);
				break;
			case REGFLAG_END_OF_TABLE :
				break;
			default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
				break;
		}
	}
}

static void init_lcm_registers(void)
{
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_get_params(LCM_PARAMS *params)
{
	pr_err("[Kernel/LCM] lcm_get_params() enter\n");

	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;
	params->lcm_if = LCM_INTERFACE_DSI0;
	params->lcm_cmd_if = LCM_INTERFACE_DSI0;

	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode = SYNC_EVENT_VDO_MODE;

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size = 256;
	params->dsi.intermediat_buffer_num = 0;	/* This para should be 0 at video mode */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.horizontal_sync_active 	= 4;
	params->dsi.horizontal_backporch  	= 132;
	params->dsi.horizontal_frontporch 	= 24;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.vertical_sync_active 	= 4;
	params->dsi.vertical_backporch  	= 8;
	params->dsi.vertical_frontporch 	= 8;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.PLL_CLOCK 		  	= 245;
	params->dsi.ssc_disable = 1;
}

static void lcm_init_lcm(void)
{
	init_lcm_registers();
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(180);

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(120);

	lcm_set_gpio_output(GPIO_LCD_STBY, GPIO_OUT_ZERO);
	lcm_vgp_supply_disable();
	MDELAY(500);

	pr_err("[Kernel/LCM] lcm_suspend() enter\n");
}

static void lcm_resume(void)
{
//	unsigned int data_array[16];

	pr_err("[Kernel/LCM] lcm_resume() enter\n");

	lcm_vgp_supply_enable();
	lcm_set_gpio_output(GPIO_LCD_STBY, GPIO_OUT_ONE);

	MDELAY(50);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);

	init_lcm_registers();

	MDELAY(33);

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
}
LCM_DRIVER b101ean02_dsi_lcm_drv = {
    .name		= "b101ean02_dsi",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.init_power		= lcm_init_power,
	.resume_power 	= lcm_resume_power,
	.suspend_power 	= lcm_suspend_power,
};
