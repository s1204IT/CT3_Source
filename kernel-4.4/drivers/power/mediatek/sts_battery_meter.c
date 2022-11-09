/*
 * Copyright (C) 2015 MediaTek Inc.
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
/*
 * Copyright (C) 2017 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * Changelog:
 *
 * 2018-Feb:  Modify driver for the battery charger.
 *
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/slab.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <linux/uaccess.h>

#include <mt-plat/mtk_boot.h>
#ifndef CONFIG_RTC_DRV_MT6397
#include <mt-plat/mtk_rtc.h>
#endif

#include <mt-plat/mtk_boot_reason.h>

#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter_hal.h>
#include <mach/mtk_battery_meter.h>
#ifdef MTK_MULTI_BAT_PROFILE_SUPPORT
#include <mach/mtk_battery_meter_table_multi_profile.h>
#else
#include <mach/mtk_battery_meter_table.h>
#endif
#include <mach/mtk_pmic.h>


#include <mt-plat/upmu_common.h>
#if defined(CONFIG_RTC_DRV_MT6397)
#include <linux/mfd/mt6397/rtc_misc.h>
#endif


/* ============================================================ // */
/* define */
/* ============================================================ // */


static DEFINE_MUTEX(FGADC_mutex);

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
int Enable_FGADC_LOG;
BATTERY_METER_CONTROL battery_meter_ctrl;
kal_bool gFG_Is_Charging = KAL_FALSE;
static unsigned int g_spm_timer = 600;
unsigned int _g_bat_sleep_total_time = NORMAL_WAKEUP_PERIOD;

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // PMIC AUXADC Related Variable */
/* ///////////////////////////////////////////////////////////////////////////////////////// */

/* SW FG */
static struct timespec g_rtc_time_before_sleep, xts_before_sleep;
static struct timespec last_oam_run_time;

void battery_meter_reset_sleep_time(void)
{
	_g_bat_sleep_total_time = 0;
}


/* ============================================================ // */
/* function prototype */
/* ============================================================ // */



/* ============================================================ // */
int get_r_fg_value(void)
{
	return 0;
}

int force_get_tbat(kal_bool update)
{
	int bat_temperature_volt = 0;

	battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &bat_temperature_volt);

	return bat_temperature_volt;
}
EXPORT_SYMBOL(force_get_tbat);

/* ============================================================ // */

signed int fgauge_read_capacity(void)
{
	int ret;
	signed int batt_capacity;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_RSOC, &batt_capacity);

	return batt_capacity;
}

signed int fgauge_read_capacity_hicharge(void)
{
	int ret;
	signed int batt_capacity;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_RSOC_HICHARGE, &batt_capacity);

	return batt_capacity;
}

void fgauge_initialization(void)
{
	unsigned int ret;

	/* 1. HW initialization */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_HW_FG_INIT, NULL);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_DUMP_REGISTER, NULL);
}

signed int get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level)
{
	return 0;
}

/* ============================================================ // */
signed int battery_meter_get_battery_voltage(kal_bool update)
{
	int ret = 0;
	int val = 5;
	static int pre_val = -1;

	if (update == KAL_TRUE || pre_val == -1) {
		val = 5;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);
		pre_val = val;
	} else {
		val = pre_val;
	}

	return val;
}

signed int battery_meter_get_battery_current(void)
{
	int ret = 0;
	signed int val = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);

	return val;
}

kal_bool battery_meter_get_battery_current_sign(void)
{
	int ret = 0;
	kal_bool val = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &val);

	return val;
}

signed int battery_meter_get_car(void)
{
	int ret = 0;
	signed int val = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &val);

	return val;
}

signed int battery_meter_get_battery_temperature(void)
{
	return force_get_tbat(KAL_TRUE);
}

signed int battery_meter_get_charger_voltage(void)
{
	int ret = 0;
	int val = 0;

	val = 5;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_CHARGER, &val);

	return val;
}

signed int battery_meter_get_battery_percentage(void)
{
    signed int ret;
    ret = fgauge_read_capacity();
    battery_log(BAT_LOG_FULL, "%s=%d\n", __FUNCTION__, ret);
    return ret;
}

signed int battery_meter_get_battery_percentage_hicharge(void)
{
    signed int ret;
    ret = fgauge_read_capacity_hicharge();
    battery_log(BAT_LOG_FULL, "%s=%d\n", __FUNCTION__, ret);
    return ret;
}

signed int battery_meter_initial(void)
{
	static kal_bool meter_initilized = KAL_FALSE;

	mutex_lock(&FGADC_mutex);
	if (meter_initilized == KAL_FALSE) {

		fgauge_initialization();
		bm_print(BM_LOG_CRTI, "[battery_meter_initial] SOC_BY_HW_FG done\n");

		meter_initilized = KAL_TRUE;
	}
	mutex_unlock(&FGADC_mutex);
	return 0;
}

void reset_parameter_car(void)
{
	int ret = 0;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_HW_RESET, NULL);
}

signed int battery_meter_reset(void)
{
	reset_parameter_car();

	return 0;
}

signed int battery_meter_sync(signed int bat_i_sense_offset)
{
	return 0;
}

signed int battery_meter_get_battery_nPercent_UI_SOC(void)
{
	return 0;
}

signed int battery_meter_get_tempR(signed int dwVolt)
{
	int TRes;

	TRes = 0;

	return TRes;
}

signed int battery_meter_get_VSense(void)
{
	int ret = 0;
	int val = 0;

	val = 1;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
	return val;
}

/* ============================================================ // */
static ssize_t fgadc_log_write(struct file *filp, const char __user *buff,
			       size_t len, loff_t *data)
{
	char proc_fgadc_data;

	if ((len <= 0) || copy_from_user(&proc_fgadc_data, buff, 1)) {
		bm_print(BM_LOG_CRTI, "fgadc_log_write error.\n");
		return -EFAULT;
	}

	if (proc_fgadc_data == '1') {
		bm_print(BM_LOG_CRTI, "enable FGADC driver log system\n");
		Enable_FGADC_LOG = BM_LOG_CRTI;
	} else if (proc_fgadc_data == '2') {
		bm_print(BM_LOG_CRTI, "enable FGADC driver log system:2\n");
		Enable_FGADC_LOG = BM_LOG_FULL;
	} else {
		bm_print(BM_LOG_CRTI, "Disable FGADC driver log system\n");
		Enable_FGADC_LOG = 0;
	}

	return len;
}

static const struct file_operations fgadc_proc_fops = {
	.write = fgadc_log_write,
};

static int init_proc_log_fg(void)
{
	int ret = 0;

#if 1
	proc_create("fgadc_log", 0644, NULL, &fgadc_proc_fops);
	bm_print(BM_LOG_CRTI, "proc_create fgadc_proc_fops\n");
#else
	proc_entry_fgadc = create_proc_entry("fgadc_log", 0644, NULL);

	if (proc_entry_fgadc == NULL) {
		ret = -ENOMEM;
		bm_print(BM_LOG_CRTI, "init_proc_log_fg: Couldn't create proc entry\n");
	} else {
		proc_entry_fgadc->write_proc = fgadc_log_write;
		bm_print(BM_LOG_CRTI, "init_proc_log_fg loaded.\n");
	}
#endif

	return ret;
}

/* ============================================================ // */

static int battery_meter_probe(struct platform_device *dev)
{
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	char *temp_strptr;
#endif
	bm_print(BM_LOG_CRTI, "[battery_meter_probe] probe\n");

	/* select battery meter control method */
	battery_meter_ctrl = bm_ctrl_cmd;
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT
	    || get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
		temp_strptr =
		    kzalloc(strlen(saved_command_line) + strlen(" androidboot.mode=charger") + 1,
			    GFP_KERNEL);
		strcpy(temp_strptr, saved_command_line);
		strcat(temp_strptr, " androidboot.mode=charger");
		saved_command_line = temp_strptr;
	}
#endif
	/* LOG System Set */
	init_proc_log_fg();

	/* last_oam_run_time = rtc_read_hw_time(); */
	get_monotonic_boottime(&last_oam_run_time);

	return 0;
}

static int battery_meter_remove(struct platform_device *dev)
{
	bm_print(BM_LOG_CRTI, "[battery_meter_remove]\n");
	return 0;
}

static void battery_meter_shutdown(struct platform_device *dev)
{

}

static int battery_meter_suspend(struct platform_device *dev, pm_message_t state)
{
	/* -- hibernation path */
	if (state.event == PM_EVENT_FREEZE) {
		pr_warn("[%s] %p:%p\n", __func__, battery_meter_ctrl, &bm_ctrl_cmd);
		battery_meter_ctrl = bm_ctrl_cmd;
	}
	/* -- end of hibernation path */
	{
		get_monotonic_boottime(&xts_before_sleep);
		get_monotonic_boottime(&g_rtc_time_before_sleep);
		if (_g_bat_sleep_total_time >= g_spm_timer)
			_g_bat_sleep_total_time = 0;
	}
	bm_print(BM_LOG_CRTI, "[battery_meter_suspend]\n");
	return 0;
}

static int battery_meter_resume(struct platform_device *dev)
{
	signed int sleep_interval;
	struct timespec rtc_time_after_sleep;

	get_monotonic_boottime(&rtc_time_after_sleep);
	sleep_interval =
		rtc_time_after_sleep.tv_sec - g_rtc_time_before_sleep.tv_sec;

	_g_bat_sleep_total_time += sleep_interval;
	battery_log(BAT_LOG_CRTI,
		"[battery_meter_resume]sleep interval=%d sleep time = %d, g_spm_timer = %d\n",
		sleep_interval, _g_bat_sleep_total_time, g_spm_timer);

	bm_print(BM_LOG_CRTI, "[battery_meter_resume]\n");
	return 0;
}

/* ----------------------------------------------------- */

#ifdef CONFIG_OF
static const struct of_device_id mt_bat_meter_of_match[] = {
	{.compatible = "mediatek,bat_meter",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_bat_meter_of_match);
#endif
static struct platform_device battery_meter_device = {
	.name = "battery_meter",
	.id = -1,
};


static struct platform_driver battery_meter_driver = {
	.probe = battery_meter_probe,
	.remove = battery_meter_remove,
	.shutdown = battery_meter_shutdown,
	.suspend = battery_meter_suspend,
	.resume = battery_meter_resume,
	.driver = {
		   .name = "battery_meter",
		   },
};

static int battery_meter_dts_probe(struct platform_device *dev)
{
	int ret = 0;
	/* struct proc_dir_entry *entry = NULL; */

	battery_log(BAT_LOG_CRTI, "******** battery_meter_dts_probe!! ********\n");

	battery_meter_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[battery_meter_dts_probe] Unable to register device (%d)\n", ret);
		return ret;
	}
	return 0;

}

static struct platform_driver battery_meter_dts_driver = {
	.probe = battery_meter_dts_probe,
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		   .name = "battery_meter_dts",
#ifdef CONFIG_OF
		   .of_match_table = mt_bat_meter_of_match,
#endif
		   },
};

static int __init battery_meter_init(void)
{
	int ret;

#ifdef CONFIG_OF
	/*  */
#else
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		bm_print(BM_LOG_CRTI, "[battery_meter_driver] Unable to device register(%d)\n",
			 ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_meter_driver);
	if (ret) {
		bm_print(BM_LOG_CRTI, "[battery_meter_driver] Unable to register driver (%d)\n",
			 ret);
		return ret;
	}
#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_meter_dts_driver);
#endif
	bm_print(BM_LOG_CRTI, "[battery_meter_driver] Initialization : DONE\n");

	return 0;

}
#ifdef BATTERY_MODULE_INIT
device_initcall(battery_meter_init);
#else
static void __exit battery_meter_exit(void)
{
}
module_init(battery_meter_init);
#endif

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("Battery Meter Device Driver");
MODULE_LICENSE("GPL");
