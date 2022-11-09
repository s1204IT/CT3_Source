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
* Copyright (C) 2017 SANYO Techno Solutions Tottori Co., Ltd.
*
* Changelog:
*
* 2018-Feb Tsutomu Nakazato <nakazato_tsutomu@sts-tottori.com> changed.
*     Add lcm device.
*/
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#else
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
#endif

#include "lcm_drv.h"

#ifndef BUILD_LK
static struct regulator *lcm_vgp;
//static struct pinctrl *lcmctrl;
//static struct pinctrl_state *lcd_pwr_high;
//static struct pinctrl_state *lcd_pwr_low;


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
#if 0
void lcm_set_gpio(int val)
{
	if (val == 0) {
		pinctrl_select_state(lcmctrl, lcd_pwr_low);
		pr_debug("Kernel/LCM: lcm set power off\n");
	} else {
		pinctrl_select_state(lcmctrl, lcd_pwr_high);
		pr_debug("Kernel/LCM: lcm set power on\n");
	}
}
#endif

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
	/* set voltage to 3.3V */
	ret = regulator_set_voltage(lcm_vgp, 3300000, 3300000);
	if (ret != 0) {
		pr_err("Kernel/LCM: lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 3300000)
		pr_err("Kernel/LCM: check regulator voltage=3300000 pass!\n");
	else
		pr_err("Kernel/LCM: check regulator voltage=3300000 fail! (voltage: %d)\n", volt);

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
	{.compatible = "mediatek,auo_lvds",},
	{}
};

static struct platform_driver lcm_driver = {
    .probe = lcm_gpio_probe,
	.driver = {
		   .name = "auo_lvds",
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
#endif
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define FRAME_WIDTH  (1280)
#define FRAME_HEIGHT (800)

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

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
   mt_set_gpio_mode(GPIO, GPIO_MODE_00);
   mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
   mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
#else
	gpio_set_value(GPIO, output);
#endif
}

static void lcm_init_power(void)
{
#ifdef BUILD_LK
#else
	pr_err("[Kernel/LCM] lcm_init_power() enter\n");
#endif
}

static void lcm_suspend_power(void)
{
#ifndef BUILD_LK
	pr_err("[Kernel/LCM] lcm_suspend_power() enter\n");
#endif
}

static void lcm_resume_power(void)
{
#ifndef BUILD_LK
	pr_err("[Kernel/LCM] lcm_resume_power() enter\n");
#endif
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_get_params() enter\n");
#else
	pr_err("[Kernel/LCM] lcm_get_params() enter\n");
#endif

    memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DPI;

    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dpi.format            = LCM_DPI_FORMAT_RGB888;
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;

    params->dpi.clk_pol           = LCM_POLARITY_RISING;	//LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width 	= 10;
    params->dpi.hsync_back_porch  	= 80;
    params->dpi.hsync_front_porch 	= 70;

    params->dpi.vsync_pulse_width 	= 2;
    params->dpi.vsync_back_porch  	= 20;
    params->dpi.vsync_front_porch 	= 19;

	params->dpi.width = FRAME_WIDTH;
	params->dpi.height = FRAME_HEIGHT;

    params->dpi.PLL_CLOCK 		  	= 76;

    params->dpi.ssc_disable = 1;

    params->dpi.lvds_tx_en = 1;
}

static void lcm_init_lcm(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_init() enter\n");
#else
	pr_err("[Kernel/LCM] lcm_init() enter\n");
#endif
}

static void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_suspend() enter\n");
#else

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
	MDELAY(30);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(30);

	lcm_set_gpio_output(GPIO_LCD_STBY, GPIO_OUT_ZERO);
	MDELAY(200);

	lcm_vgp_supply_disable();

	pr_err("[Kernel/LCM] lcm_suspend() enter\n");
#endif
}

static void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_resume() enter\n");
#else
	pr_err("[Kernel/LCM] lcm_resume() enter\n");

	lcm_vgp_supply_enable();
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_STBY, GPIO_OUT_ONE);
	MDELAY(40);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(100);

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
#endif
}
LCM_DRIVER auo_lvds_lcm_drv = {
    .name		= "auo_lvds",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.init_power		= lcm_init_power,
	.resume_power 	= lcm_resume_power,
	.suspend_power 	= lcm_suspend_power,
};
