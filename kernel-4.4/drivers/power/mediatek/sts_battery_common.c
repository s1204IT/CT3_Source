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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    sts_battery_common.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of mt6323 Battery charging algorithm
 *   and the Anroid Battery service for updating the battery status
 *
 * Author:
 * -------
 * Oscar Liu
 *
 ****************************************************************************/

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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/irq.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#endif
#include <linux/suspend.h>
#include <linux/reboot.h>

#include <linux/gpio.h>

#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/irq.h>

#include <mt-plat/mtk_boot.h>
#ifndef CONFIG_RTC_DRV_MT6397
#include <mt-plat/mtk_rtc.h>
#endif
#include <mach/mtk_charging.h>
#include <mt-plat/upmu_common.h>

#include <mt-plat/charging.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mach/mtk_battery_meter.h>
#include <mach/mtk_charging.h>
#include <mach/mtk_pmic.h>

#if defined(CONFIG_RTC_DRV_MT6397)
#include <linux/mfd/mt6397/rtc_misc.h>
#endif
/* ////////////////////////////////////////////////////////////////////////////// */
/* Battery Logging Entry */
/* ////////////////////////////////////////////////////////////////////////////// */
int Enable_BATDRV_LOG = BAT_LOG_CRTI;

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Smart Battery Structure */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
PMU_ChargerStruct BMT_status;

/*
 *  Global Variable
 */

struct wake_lock battery_suspend_lock;
CHARGING_CONTROL battery_charging_control;
static kal_bool g_bat_init_flag;
int g_platform_boot_mode;
struct timespec g_bat_time_before_sleep;
static kal_bool g_charger_hv_detect_reset = KAL_FALSE;

static unsigned int g_batt_temp_status = TEMP_POS_NORMAL;
static kal_bool g_charger_hv_status = KAL_FALSE;
static kal_bool battery_suspended = KAL_FALSE;

struct battery_custom_data batt_cust_data;

#define	DEFAULT_BAT_CAPACITY	50
#define	DEFAULT_BAT_VOL			4164		/* mv*/
#define	DEFAULT_BAT_TEMP		20

/*
 *  Thread related
 */
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
static kal_bool bat_thread_timeout = KAL_FALSE;
static DEFINE_MUTEX(bat_mutex);
static DEFINE_MUTEX(charger_type_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_thread_wq);
static struct hrtimer charger_hv_detect_timer;
static kal_bool charger_hv_detect_flag = KAL_FALSE;
static DECLARE_WAIT_QUEUE_HEAD(charger_hv_detect_waiter);
static struct hrtimer battery_kthread_timer;
kal_bool g_battery_soc_ready = KAL_FALSE;
static kal_bool fg_battery_shutdown;
static kal_bool fg_bat_thread;
static kal_bool fg_hv_thread;

static int chrdet_irq;

static int chgled_gpio;

/* ////////////////////////////////////////////////////////////////////////////// */
/* FOR ANDROID BATTERY SERVICE */
/* ////////////////////////////////////////////////////////////////////////////// */

struct ac_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int AC_ONLINE;
};

struct usb_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int USB_ONLINE;
};

struct battery_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int BAT_STATUS;
	int BAT_HEALTH;
	int BAT_PRESENT;
	int BAT_TECHNOLOGY;
	int BAT_CAPACITY;
	/* Add for Battery Service */
	int BAT_batt_vol;
	int BAT_batt_temp;
#if 0
	/* Add for EM */
	int BAT_TemperatureR;
	int BAT_TempBattVoltage;
	int BAT_InstatVolt;
	int BAT_BatteryAverageCurrent;
	int BAT_BatterySenseVoltage;
	int BAT_ISenseVoltage;
	int BAT_ChargerVoltage;
#endif
#if 0
	/* Dual battery */
	int status_smb;
	int capacity_smb;
	int present_smb;
#endif
	int adjust_power;
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_batt_vol,
	POWER_SUPPLY_PROP_batt_temp,
#if 0
	/* Add for EM */
	POWER_SUPPLY_PROP_TemperatureR,
	POWER_SUPPLY_PROP_TempBattVoltage,
	POWER_SUPPLY_PROP_InstatVolt,
	POWER_SUPPLY_PROP_BatteryAverageCurrent,
	POWER_SUPPLY_PROP_BatterySenseVoltage,
	POWER_SUPPLY_PROP_ISenseVoltage,
	POWER_SUPPLY_PROP_ChargerVoltage,
#endif
#if 0
	/* Dual battery */
	POWER_SUPPLY_PROP_status_smb,
	POWER_SUPPLY_PROP_capacity_smb,
	POWER_SUPPLY_PROP_present_smb,
#endif
	/* ADB CMD Discharging */
	POWER_SUPPLY_PROP_adjust_power,
};

int read_tbat_value(void)
{
	return BMT_status.temperature;
}

int get_charger_detect_status(void)
{
	kal_bool chr_status;

	battery_charging_control(CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);
	return chr_status;
}

bool is_force_kpoc_boot(void)
{
	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT
	    || g_platform_boot_mode == RECOVERY_BOOT){
		return	0;
	}else{
		return	1;
	}
}

extern void rtc_set_hicharge(int on);
extern u32 rtc_get_hicharge(void);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // PMIC PCHR Related APIs */
/* ///////////////////////////////////////////////////////////////////////////////////////// */

kal_bool upmu_is_chr_det(void)
{
	unsigned int tmp32;

	if (battery_charging_control == NULL)
		battery_charging_control = chr_control_interface;

	tmp32 = get_charger_detect_status();

	if (tmp32 == 0)
		return KAL_FALSE;

	return KAL_TRUE;
}
EXPORT_SYMBOL(upmu_is_chr_det);

static void bat_thread_wakeup_inner(void)
{
	bat_thread_timeout = KAL_TRUE;

	battery_meter_reset_sleep_time();

	wake_up(&bat_thread_wq);
}

static void bat_thread_wakeup(void)
{
	battery_log(BAT_LOG_FULL, "******** battery : %s  ********\n", __func__);

	bat_thread_wakeup_inner();
}

void wake_up_bat(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] %s. \r\n", __func__);

	bat_thread_wakeup_inner();
}
EXPORT_SYMBOL(wake_up_bat);


static ssize_t bat_log_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	char proc_bat_data;

	if ((len <= 0) || copy_from_user(&proc_bat_data, buff, 1)) {
		battery_log(BAT_LOG_FULL, "bat_log_write error.\n");
		return -EFAULT;
	}

	if (proc_bat_data == '1') {
		battery_log(BAT_LOG_CRTI, "enable battery driver log system\n");
		Enable_BATDRV_LOG = 1;
	} else if (proc_bat_data == '2') {
		battery_log(BAT_LOG_CRTI, "enable battery driver log system:2\n");
		Enable_BATDRV_LOG = 2;
	} else {
		battery_log(BAT_LOG_CRTI, "Disable battery driver log system\n");
		Enable_BATDRV_LOG = 0;
	}

	return len;
}

static const struct file_operations bat_proc_fops = {
	.write = bat_log_write,
};

static int init_proc_log(void)
{
	int ret = 0;

	proc_create("batdrv_log", 0644, NULL, &bat_proc_fops);
	battery_log(BAT_LOG_CRTI, "proc_create bat_proc_fops\n");
	return ret;
}

static int ac_get_property(struct power_supply *psy,
			   enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct ac_data *data = container_of(psy->desc, struct ac_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->AC_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int usb_get_property(struct power_supply *psy,
			    enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy->desc, struct usb_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->USB_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int battery_get_property(struct power_supply *psy,
				enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct battery_data *data = container_of(psy->desc, struct battery_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->BAT_STATUS;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->BAT_HEALTH;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->BAT_PRESENT;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = data->BAT_TECHNOLOGY;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->BAT_CAPACITY;
		break;
	case POWER_SUPPLY_PROP_batt_vol:
		val->intval = data->BAT_batt_vol;
		break;
	case POWER_SUPPLY_PROP_batt_temp:
		val->intval = data->BAT_batt_temp;
		break;
#if 0
	case POWER_SUPPLY_PROP_TemperatureR:
		val->intval = data->BAT_TemperatureR;
		break;
	case POWER_SUPPLY_PROP_TempBattVoltage:
		val->intval = data->BAT_TempBattVoltage;
		break;
	case POWER_SUPPLY_PROP_InstatVolt:
		val->intval = data->BAT_InstatVolt;
		break;
	case POWER_SUPPLY_PROP_BatteryAverageCurrent:
		val->intval = data->BAT_BatteryAverageCurrent;
		break;
	case POWER_SUPPLY_PROP_BatterySenseVoltage:
		val->intval = data->BAT_BatterySenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ISenseVoltage:
		val->intval = data->BAT_ISenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ChargerVoltage:
		val->intval = data->BAT_ChargerVoltage;
		break;
#endif
#if 0
		/* Dual battery */
	case POWER_SUPPLY_PROP_status_smb:
		val->intval = data->status_smb;
		break;
	case POWER_SUPPLY_PROP_capacity_smb:
		val->intval = data->capacity_smb;
		break;
	case POWER_SUPPLY_PROP_present_smb:
		val->intval = data->present_smb;
		break;
#endif
	case POWER_SUPPLY_PROP_adjust_power:
		val->intval = data->adjust_power;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* ac_data initialization */
static struct ac_data ac_main = {
	.psd = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = ac_get_property,
		},
	.AC_ONLINE = 0,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psd = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = usb_get_property,
		},
	.USB_ONLINE = 0,
};

/* battery_data initialization */
static struct battery_data battery_main = {
	.psd = {
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_props,
		.num_properties = ARRAY_SIZE(battery_props),
		.get_property = battery_get_property,
		},
/* CC: modify to have a full power supply status */
	.BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = 50,
	.BAT_batt_vol = 0,
	.BAT_batt_temp = 0,
#if 0
	/* Dual battery */
	.status_smb = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.capacity_smb = 50,
	.present_smb = 0,
#endif
	/* ADB CMD discharging */
	.adjust_power = -1,
};

#if 0
static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;	/* API */
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV;	/* API */
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;	/* VBAT */
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense;	/* API */
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;
	if ((BMT_status.UI_SOC == 100) && (BMT_status.charger_exist == KAL_TRUE))
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
}
#endif

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = bat_data->psy;

	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;

	if(g_charger_hv_status == TRUE)
		bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		
	else if(g_batt_temp_status == TEMP_POS_HIGH)
		bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_OVERHEAT;
		
	else if(g_batt_temp_status == TEMP_POS_LOW)
		bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_COLD;

	bat_data->BAT_batt_vol = BMT_status.bat_vol;
	bat_data->BAT_batt_temp = BMT_status.temperature * 10;

	if ((BMT_status.charger_exist == KAL_TRUE) && (BMT_status.bat_charging_state != CHR_ERROR)) {
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
	} else {		/* Only Battery */
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
#if 1
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	if ((BMT_status.UI_SOC == 100) && (BMT_status.charger_exist == KAL_TRUE))
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
#else
	mt_battery_update_EM(bat_data);
#endif

	power_supply_changed(bat_psy);
}

static void ac_update(struct ac_data *ac_data)
{
	static int ac_status = -1;
	struct power_supply *ac_psy = ac_data->psy;
	struct power_supply_desc *ac_psd = &ac_data->psd;

	if (BMT_status.charger_exist == KAL_TRUE) {
		if ((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
		    (BMT_status.charger_type == STANDARD_CHARGER)) {
			ac_data->AC_ONLINE = 1;
			ac_psd->type = POWER_SUPPLY_TYPE_MAINS;
		} else {
			ac_data->AC_ONLINE = 0;
		}
	} else {
		ac_data->AC_ONLINE = 0;
	}

//printk("!!!!!!!!!ac_update %d,%d,%d\n",BMT_status.charger_exist,BMT_status.charger_type,ac_data->AC_ONLINE);

	if (ac_status != ac_data->AC_ONLINE) {
		ac_status = ac_data->AC_ONLINE;
		power_supply_changed(ac_psy);
	}
}

static void usb_update(struct usb_data *usb_data)
{
	static int usb_status = -1;
	struct power_supply *usb_psy = usb_data->psy;
	struct power_supply_desc *usb_psd = &usb_data->psd;

	if (BMT_status.charger_exist == KAL_TRUE) {
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
			usb_data->USB_ONLINE = 1;
			usb_psd->type = POWER_SUPPLY_TYPE_USB;
		} else {
			usb_data->USB_ONLINE = 0;
		}
	} else {
		usb_data->USB_ONLINE = 0;
	}

//printk("!!!!!!!!!usb_update %d,%d,%d\n",BMT_status.charger_exist,BMT_status.charger_type,usb_data->USB_ONLINE);

	if (usb_status != usb_data->USB_ONLINE) {
		usb_status = usb_data->USB_ONLINE;
		power_supply_changed(usb_psy);
	}
}

static void leds_update(void)
{	
	kal_bool chg_status = KAL_FALSE;

	if ((BMT_status.charger_exist == KAL_TRUE) && (BMT_status.bat_charging_state != CHR_ERROR)) {
		if((BMT_status.UI_SOC <= 99)){
			chg_status = KAL_TRUE;
		}
	}
	if( chg_status == KAL_FALSE){
		gpio_direction_output( chgled_gpio, 0);
		battery_log(BAT_LOG_CRTI, "%s[%d] LED OFF!!!!!!!\n", __func__,__LINE__);
	}else{
		gpio_direction_output( chgled_gpio, 1);
		battery_log(BAT_LOG_CRTI, "%s[%d] LED ON!!!!!!!\n", __func__,__LINE__);
	}
}

static void mt_battery_GetBatteryData_batThread(void)
{
	unsigned int bat_vol, charger_vol, Vsense;
	signed int temperature, SOC;

	bat_vol = battery_meter_get_battery_voltage(KAL_TRUE);
	Vsense = battery_meter_get_VSense();
	charger_vol = battery_meter_get_charger_voltage();
	temperature = battery_meter_get_battery_temperature();

	if(BMT_status.hicharge)
		SOC = battery_meter_get_battery_percentage_hicharge();
	else
		SOC = battery_meter_get_battery_percentage();

	if(temperature != INT_MIN){
		BMT_status.temperature = temperature;
	}else{
		battery_log(BAT_LOG_CRTI, "battery_meter_get_battery_temperature error.\n");
	}

	if(bat_vol != INT_MIN){
		BMT_status.bat_vol = bat_vol;
	}else{
		battery_log(BAT_LOG_CRTI, "battery_meter_get_battery_voltage error.\n");
	}
	BMT_status.Vsense = Vsense;
	BMT_status.charger_vol = charger_vol;
	
	if(SOC != INT_MIN){
		BMT_status.SOC = SOC;
		BMT_status.UI_SOC = SOC;
	}else{
		battery_log(BAT_LOG_CRTI, "battery_meter_get_battery_percentage error.\n");
	}

	if (g_battery_soc_ready == KAL_FALSE)
		g_battery_soc_ready = KAL_TRUE;

	if(BMT_status.charger_exist == KAL_TRUE)
		battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

	battery_log(BAT_LOG_CRTI,
		"Vbat=%d,VChr=%d,Tbat=%d,SOC=%d,UI_SOC=%d\n",
		BMT_status.bat_vol,
		BMT_status.charger_vol, BMT_status.temperature,
		BMT_status.SOC, BMT_status.UI_SOC);
}

static void mt_battery_GetBatteryData_hvThread(void)
{
	kal_bool hv_status = KAL_FALSE;

	if (chargin_hw_init_done) {
		battery_charging_control(CHARGING_CMD_GET_HV_STATUS, &hv_status);
//		if(BMT_status.charger_exist == KAL_TRUE)
//			battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);
	}

	BMT_status.hv_status = hv_status;
}

static void mt_battery_CheckBatteryTemp(void)
{
	if (BMT_status.temperature < batt_cust_data.min_charge_temperature) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
		g_batt_temp_status = TEMP_POS_LOW;
		BMT_status.bat_charging_state = CHR_ERROR;
	}
	else if (g_batt_temp_status == TEMP_POS_LOW) {
		if (BMT_status.temperature >=
		    batt_cust_data.min_charge_temperature_plus_x_degree) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature raise from %d to %d(%d), allow charging!!\n\r",
				    batt_cust_data.min_charge_temperature,
				    BMT_status.temperature,
				    batt_cust_data.min_charge_temperature_plus_x_degree);
			g_batt_temp_status = TEMP_POS_NORMAL;
		    BMT_status.bat_charging_state = CHR_PRE;
		}
	}
	else if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Battery Over Temperature !!\n\r");
		g_batt_temp_status = TEMP_POS_HIGH;
		BMT_status.bat_charging_state = CHR_ERROR;
	}
	else if (g_batt_temp_status == TEMP_POS_HIGH) {
		if (BMT_status.temperature < batt_cust_data.max_charge_temperature_minus_x_degree) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature down from %d to %d(%d), allow charging!!\n\r",
				    batt_cust_data.max_charge_temperature, BMT_status.temperature,
				    batt_cust_data.max_charge_temperature_minus_x_degree);
			g_batt_temp_status = TEMP_POS_NORMAL;
		    BMT_status.bat_charging_state = CHR_PRE;
		}
	}
	else {
		g_batt_temp_status = TEMP_POS_NORMAL;
	}
}

static void mt_battery_CheckHvStatus(void)
{
	if (BMT_status.charger_exist == KAL_TRUE) {
		if (g_charger_hv_detect_reset == KAL_TRUE) {
			g_charger_hv_status = FALSE;
		}
		else if (BMT_status.hv_status == KAL_TRUE) {
			g_charger_hv_status = TRUE;
		}
	}
	else {
		g_charger_hv_status = FALSE;
	}
	g_charger_hv_detect_reset = KAL_FALSE;
}

static void mt_battery_CheckBatteryStatus_hvThread(void)
{
	mt_battery_CheckHvStatus();

	if (g_charger_hv_status == TRUE) {
		BMT_status.bat_charging_state = CHR_ERROR;
	}
	else if (BMT_status.bat_charging_state == CHR_ERROR) {
		BMT_status.bat_charging_state = CHR_PRE;
	}
}

static void mt_battery_thermal_check(void)
{
	if (BMT_status.temperature >= 60) {
		if ((g_platform_boot_mode == META_BOOT)
			    || (g_platform_boot_mode == ADVMETA_BOOT)
			    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			battery_log(BAT_LOG_FULL,
				    "[BATTERY] boot mode = %d, bypass temperature check\n",
				    g_platform_boot_mode);
		} else {
			struct battery_data *bat_data = &battery_main;
			struct power_supply *bat_psy = bat_data->psy;

			battery_log(BAT_LOG_CRTI,
				    "[Battery] Tbat(%d)>=60, system need power down.\n",
				    BMT_status.temperature);

			bat_data->BAT_CAPACITY = 0;

			power_supply_changed(bat_psy);

			if (BMT_status.charger_exist == KAL_TRUE) {
				/* can not power down due to charger exist, so need reset system */
				orderly_reboot();
			}
			/* avoid SW no feedback */
			orderly_poweroff(true);
		}
	}
}

static void mt_battery_update_status(void)
{
	battery_log(BAT_LOG_FULL, "mt_battery_update_status.\n");
	usb_update(&usb_main);
	ac_update(&ac_main);
	battery_update(&battery_main);
}


CHARGER_TYPE mt_charger_type_detection(void)
{
	CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;

	mutex_lock(&charger_type_mutex);

	if (BMT_status.charger_type == CHARGER_UNKNOWN) {
		battery_charging_control(CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
		BMT_status.charger_type = CHR_Type_num;
	}
	mutex_unlock(&charger_type_mutex);

	return BMT_status.charger_type;
}

CHARGER_TYPE mt_get_charger_type(void)
{
	return BMT_status.charger_type;
}

static void mt_kpoc_power_off_check(void)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	battery_log(BAT_LOG_FULL,
		    "[mt_kpoc_power_off_check] , chr_vol=%d, boot_mode=%d\r\n",
		    BMT_status.charger_vol, g_platform_boot_mode);
	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		if ((upmu_is_chr_det() == KAL_FALSE) && (BMT_status.charger_vol < 2500)) {	/* vbus < 2.5V */
			battery_log(BAT_LOG_CRTI,
				    "[bat_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
			orderly_poweroff(true);
		}
	}
#endif
}

#define	MAINT_OFF_CHARGING_AUTO_POWER_ON_PERCENT	5

void do_chrdet_int_task(void)
{
	int	SOC;
	if (g_bat_init_flag == KAL_TRUE) {
		if (upmu_is_chr_det() == KAL_TRUE) {
//			if (g_platform_boot_mode != KERNEL_POWER_OFF_CHARGING_BOOT
//			    && g_platform_boot_mode != LOW_POWER_OFF_CHARGING_BOOT) {
				wake_lock(&battery_suspend_lock);
//			}
			battery_log(BAT_LOG_CRTI, "[do_chrdet_int_task] charger exist!\n");

			BMT_status.charger_exist = KAL_TRUE;
			BMT_status.charger_type = CHARGER_UNKNOWN;
//			if (BMT_status.charger_type == CHARGER_UNKNOWN) {
				mt_charger_type_detection();
//			}
		} else {
			battery_log(BAT_LOG_CRTI, "[do_chrdet_int_task] charger NOT exist!\n");

			if(BMT_status.charger_exist != KAL_FALSE) {
				if(BMT_status.SOC >= 50) {
					rtc_set_hicharge(1);
					BMT_status.hicharge = KAL_TRUE;
				}
			}

			BMT_status.charger_exist = KAL_FALSE;
			BMT_status.charger_type = CHARGER_UNKNOWN;
			BMT_status.bat_full = KAL_FALSE;
			BMT_status.bat_in_recharging_state = KAL_FALSE;
			BMT_status.bat_charging_state = CHR_PRE;
			BMT_status.total_charging_time = 0;
			BMT_status.PRE_charging_time = 0;
			BMT_status.CC_charging_time = 0;
			BMT_status.TOPOFF_charging_time = 0;
			BMT_status.POSTFULL_charging_time = 0;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
				battery_log(BAT_LOG_CRTI,
					    "[pmic_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				orderly_poweroff(true);
			}
#endif

//			if (g_platform_boot_mode != KERNEL_POWER_OFF_CHARGING_BOOT
//			    && g_platform_boot_mode != LOW_POWER_OFF_CHARGING_BOOT) {
				wake_unlock(&battery_suspend_lock);
//			}
		}

		/* Place charger detection and battery update here is used to speed up charging icon display. */

		if (BMT_status.UI_SOC == 100 && BMT_status.charger_exist == KAL_TRUE) {
//			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = KAL_TRUE;
		}

		if (g_battery_soc_ready == KAL_FALSE) {
			SOC = battery_meter_get_battery_percentage();
			if( SOC != INT_MIN ){
				BMT_status.SOC = SOC;
			}
		}

		if (BMT_status.bat_vol > 0) {
			mt_battery_update_status();
		}

		wake_up_bat();
		{
			ktime_t ktime;

			ktime = ktime_set(0, BAT_MS_TO_NS(300));
			hrtimer_cancel(&charger_hv_detect_timer);
			g_charger_hv_detect_reset = KAL_TRUE;
			hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);
		}
	} else {
		battery_log(BAT_LOG_CRTI,
			    "[do_chrdet_int_task] battery thread not ready, will do after bettery init.\n");
	}

}

static irqreturn_t ops_chrdet_int_handler(int irq, void *dev_id)
{
//	battery_log(BAT_LOG_FULL, "[Power/Battery][chrdet_bat_int_handler]....\n");

	do_chrdet_int_task();

	return IRQ_HANDLED;
}

static void BAT_thread(void)
{
	static kal_bool battery_meter_initilized = KAL_FALSE;

	if (battery_meter_initilized == KAL_FALSE) {
		battery_meter_initial();	/* move from battery_probe() to decrease booting time */
		battery_meter_initilized = KAL_TRUE;
	}

	if (fg_battery_shutdown)
		return;

	mt_battery_GetBatteryData_batThread();
	if (fg_battery_shutdown)
		return;

	mt_battery_thermal_check();

	mt_battery_CheckBatteryTemp();

	if (BMT_status.charger_exist == KAL_TRUE && !fg_battery_shutdown) {
		mt_battery_charging_algorithm();
	}

	mt_battery_update_status();
	mt_kpoc_power_off_check();
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Internal API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */

static int bat_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 10s, 10* 1000 ms */

#if defined(BATTERY_SW_INIT)
		battery_charging_control(CHARGING_CMD_SW_INIT, NULL);
#endif

	/* Run on a process content */
	while (!fg_battery_shutdown) {
		wait_event(bat_thread_wq, (bat_thread_timeout == KAL_TRUE));
		bat_thread_timeout = KAL_FALSE;
		hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);

		mutex_lock(&bat_mutex);

		if ((chargin_hw_init_done == KAL_TRUE) && (battery_suspended == KAL_FALSE))
			BAT_thread();

		mutex_unlock(&bat_mutex);

		battery_log(BAT_LOG_FULL, "wait event\n");
	}

	mutex_lock(&bat_mutex);
	fg_bat_thread = KAL_TRUE;
	mutex_unlock(&bat_mutex);

	return 0;
}

static int charger_hv_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime = ktime_set(1, 0);

	while (!fg_battery_shutdown) {
		wait_event_interruptible(charger_hv_detect_waiter,
					 (charger_hv_detect_flag == KAL_TRUE));
		charger_hv_detect_flag = KAL_FALSE;

		if (fg_battery_shutdown)
			break;

		mt_battery_GetBatteryData_hvThread();
		mt_battery_CheckBatteryStatus_hvThread();
		if (BMT_status.bat_charging_state == CHR_ERROR) {
			BAT_BatteryStatusFailAction();
		}

		leds_update();

		hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);
	}

	fg_hv_thread = KAL_TRUE;

	pr_info( "[%s] end.\n", __func__);

	return 0;
}

static enum hrtimer_restart charger_hv_detect_sw_workaround(struct hrtimer *timer)
{
	charger_hv_detect_flag = KAL_TRUE;
	wake_up_interruptible(&charger_hv_detect_waiter);

	battery_log(BAT_LOG_FULL, "[%s]\n", __func__);

	return HRTIMER_NORESTART;
}

static void charger_hv_detect_sw_workaround_init(void)
{
	ktime_t ktime;
	struct task_struct *th;

	ktime = ktime_set(2, 0);
	hrtimer_init(&charger_hv_detect_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	charger_hv_detect_timer.function = charger_hv_detect_sw_workaround;
	hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);

	th = kthread_run(charger_hv_detect_sw_thread_handler, 0, "mtk charger_hv_detect_sw_workaround");
	if (IS_ERR(th)) {
		battery_log(BAT_LOG_FULL,
			    "[%s]: failed to create charger_hv_detect_sw_thread_handler thread\n",
			    __func__);
	}
	battery_log(BAT_LOG_CRTI, "%s : done\n", __func__);
}


static enum hrtimer_restart battery_kthread_hrtimer_func(struct hrtimer *timer)
{
	bat_thread_wakeup();

	return HRTIMER_NORESTART;
}

static void battery_kthread_init(void)
{
	ktime_t ktime;
	struct task_struct *th;

	ktime = ktime_set(0, 0);
	hrtimer_init(&battery_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	battery_kthread_timer.function = battery_kthread_hrtimer_func;
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);

	th = kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread");
	if (IS_ERR(th)) {
		battery_log(BAT_LOG_FULL,
			    "[%s]: failed to create bat_thread_kthread thread\n",
			    __func__);
	}
	battery_log(BAT_LOG_CRTI, "%s : done\n", __func__);
}


static void get_charging_control(void)
{
	battery_charging_control = chr_control_interface;
}

static int battery_probe(struct platform_device *dev)
{
	int ret = 0;
	int SOC;

	battery_log(BAT_LOG_CRTI, "******** battery driver probe!! ********\n");

	get_charging_control();

	battery_charging_control(CHARGING_CMD_GET_PLATFORM_BOOT_MODE, &g_platform_boot_mode);
	battery_log(BAT_LOG_CRTI, "[BAT_probe] g_platform_boot_mode = %d\n ", g_platform_boot_mode);

	wake_lock_init(&battery_suspend_lock, WAKE_LOCK_SUSPEND, "battery suspend wakelock");

	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		wake_lock(&battery_suspend_lock);
	}

	/* Integrate with Android Battery Service */
	ac_main.psy = power_supply_register(&(dev->dev), &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register AC Fail !!\n");
		ret = PTR_ERR(ac_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register AC Success !!\n");

	usb_main.psy = power_supply_register(&(dev->dev), &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register USB Fail !!\n");
		ret = PTR_ERR(usb_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register USB Success !!\n");

	battery_main.psy = power_supply_register(&(dev->dev), &battery_main.psd, NULL);
	if (IS_ERR(battery_main.psy)) {
		battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(battery_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI, "[BAT_probe] power_supply_register Battery Success !!\n");

	/* Initialization BMT Struct */
	BMT_status.bat_exist = KAL_TRUE;	/* phone must have battery */
	BMT_status.charger_exist = KAL_FALSE;	/* for default, no charger */
	BMT_status.bat_vol = 0;
	BMT_status.ICharging = 0;
	BMT_status.temperature = DEFAULT_BAT_TEMP;
	BMT_status.temperatureR = 0;
	BMT_status.temperatureV = 0;
	BMT_status.charger_vol = 0;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.SOC = DEFAULT_BAT_CAPACITY;
	BMT_status.UI_SOC = DEFAULT_BAT_CAPACITY;

	BMT_status.bat_charging_state = CHR_PRE;
	BMT_status.bat_in_recharging_state = KAL_FALSE;
	BMT_status.bat_full = KAL_FALSE;
	BMT_status.nPercent_ZCV = 0;
	BMT_status.nPrecent_UI_SOC_check_point = 0;
	BMT_status.hv_status = KAL_FALSE;

	BMT_status.hicharge = KAL_FALSE;
	if(rtc_get_hicharge() != 0) {
		SOC = battery_meter_get_battery_percentage();
		if( SOC == INT_MIN ){
			SOC = DEFAULT_BAT_TEMP;
		}
		if(SOC < 20) {
			rtc_set_hicharge(0);
		}
		else {
			BMT_status.hicharge = KAL_TRUE;
		}
	}

	BMT_status.charger_type = CHARGER_UNKNOWN;
	if (upmu_is_chr_det() == KAL_TRUE) {
		BMT_status.charger_exist = KAL_TRUE;
		mt_charger_type_detection();
		wake_lock(&battery_suspend_lock);
	}
	else {
		BMT_status.charger_exist = KAL_FALSE;
	}

	batt_cust_data.max_charge_temperature = 48;
	batt_cust_data.max_charge_temperature_minus_x_degree = 48;
	batt_cust_data.min_charge_temperature = 0;
	batt_cust_data.min_charge_temperature_plus_x_degree = 0;

	battery_kthread_init();
	charger_hv_detect_sw_workaround_init();

	/*LOG System Set */
	init_proc_log();

	g_bat_init_flag = KAL_TRUE;

	return 0;
}

static void battery_timer_pause(void)
{
	mutex_lock(&bat_mutex);

	/* cancel timer */
	hrtimer_cancel(&battery_kthread_timer);
	hrtimer_cancel(&charger_hv_detect_timer);

	battery_suspended = KAL_TRUE;
	mutex_unlock(&bat_mutex);

	battery_log(BAT_LOG_CRTI, "@bs=1@\n");

	get_monotonic_boottime(&g_bat_time_before_sleep);
}

static void battery_timer_resume(void)
{
	struct timespec bat_time_after_sleep;
	ktime_t ktime, hvtime;

	ktime = ktime_set(1, 0);	/* 1s */
	hvtime = ktime_set(2, 0);

	get_monotonic_boottime(&bat_time_after_sleep);

	mutex_lock(&bat_mutex);

	/* restore timer */
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
	hrtimer_start(&charger_hv_detect_timer, hvtime, HRTIMER_MODE_REL);

	if (chargin_hw_init_done)
		if(BMT_status.charger_exist == KAL_TRUE)
			battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

	if(BMT_status.hicharge)
		BMT_status.UI_SOC = battery_meter_get_battery_percentage_hicharge();
	else
		BMT_status.UI_SOC = battery_meter_get_battery_percentage();

	battery_suspended = KAL_FALSE;
	battery_log(BAT_LOG_CRTI, "@bs=0@\n");

	mutex_unlock(&bat_mutex);

	leds_update();
}

static int battery_remove(struct platform_device *dev)
{
	battery_log(BAT_LOG_CRTI, "******** battery driver remove!! ********\n");

	return 0;
}

static void battery_shutdown(struct platform_device *dev)
{
	int count = 0;

	mutex_lock(&bat_mutex);
	fg_battery_shutdown = KAL_TRUE;
	wake_up_bat();
	charger_hv_detect_flag = KAL_TRUE;
	wake_up_interruptible(&charger_hv_detect_waiter);

	while ((!fg_bat_thread || !fg_hv_thread) && count < 5) {
		mutex_unlock(&bat_mutex);
		msleep(20);
		count++;
		mutex_lock(&bat_mutex);
	}

	if (!fg_bat_thread || !fg_hv_thread)
		battery_log(BAT_LOG_CRTI, "failed to terminate battery related thread(%d, %d)\n",
			    fg_bat_thread, fg_hv_thread);

	mutex_unlock(&bat_mutex);
}

#ifdef CONFIG_OF
static const struct of_device_id mt_battery_of_match[] = {
	{.compatible = "mediatek,battery",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_battery_of_match);
#endif

static int battery_pm_suspend(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_resume(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_freeze(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore_noirq(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static const struct dev_pm_ops battery_pm_ops = {
	.suspend = battery_pm_suspend,
	.resume = battery_pm_resume,
	.freeze = battery_pm_freeze,
	.thaw = battery_pm_restore,
	.restore = battery_pm_restore,
	.restore_noirq = battery_pm_restore_noirq,
};

#if defined(CONFIG_OF) || defined(BATTERY_MODULE_INIT)
static struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};
#endif

static struct platform_driver battery_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	.driver = {
		   .name = "battery",
		   .pm = &battery_pm_ops,
		   },
};

#ifdef CONFIG_OF
static int battery_dts_probe(struct platform_device *dev)
{
	int ret = 0;

	battery_log(BAT_LOG_CRTI, "******** battery_dts_probe!! ********\n");

	chrdet_irq = platform_get_irq(dev, 0);
	if (chrdet_irq <= 0)
		pr_err("******** don't support irq from dts ********\n");
	else {
		ret = request_threaded_irq(chrdet_irq, NULL,
					   ops_chrdet_int_handler,
					   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT, "ops_pmic_chrdet", dev);
		if (ret) {
			pr_err("%s: request_threaded_irq err = %d\n", __func__, ret);
			return ret;
		}
		irq_set_irq_wake(chrdet_irq, 1);
	}

	battery_device.dev.of_node = dev->dev.of_node;

	chgled_gpio = -1;
#ifdef CONFIG_OF
	chgled_gpio = of_get_named_gpio( battery_device.dev.of_node, "chgled-gpio", 0);
	pr_info( "[%s] of_get_named_gpio( chgled): %d\n", __func__, chgled_gpio);

	if(chgled_gpio >= 0) {
		gpio_request( chgled_gpio, NULL);
//		gpio_direction_output( chgled_gpio, 0);	// Low: active
	}
#endif

	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[battery_dts_probe] Unable to register device (%d)\n", ret);
		return ret;
	}

	return 0;

}

static struct platform_driver battery_dts_driver = {
	.probe = battery_dts_probe,
	.remove = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "battery-dts",
#ifdef CONFIG_OF
		   .of_match_table = mt_battery_of_match,
#endif
		   },
};

#endif
/* -------------------------------------------------------- */

static int battery_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
	case PM_RESTORE_PREPARE:	/* Going to restore a saved image */
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_pause();
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:	/* Hibernation finished */
	case PM_POST_SUSPEND:	/* Suspend finished */
	case PM_POST_RESTORE:	/* Restore failed */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block battery_pm_notifier_block = {
	.notifier_call = battery_pm_event,
	.priority = 0,
};

static int __init battery_init(void)
{
	int ret;

	battery_log(BAT_LOG_FULL, "battery_init\n");

#ifdef CONFIG_OF
	/*  */
#else

#ifdef BATTERY_MODULE_INIT
	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[battery_device] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
			    "****[battery_driver] Unable to register driver (%d)\n", ret);
		return ret;
	}

#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_dts_driver);
#endif
	ret = register_pm_notifier(&battery_pm_notifier_block);
	if (ret)
		battery_log(BAT_LOG_FULL, "[%s] failed to register PM notifier %d\n", __func__, ret);

	battery_log(BAT_LOG_CRTI, "****[battery_driver] Initialization : DONE !!\n");
	return 0;
}

#ifdef BATTERY_MODULE_INIT
late_initcall(battery_init);
#else
static void __exit battery_exit(void)
{
}
late_initcall(battery_init);

#endif

MODULE_AUTHOR("Oscar Liu");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
