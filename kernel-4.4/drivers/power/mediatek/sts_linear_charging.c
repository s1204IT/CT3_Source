/*
 *  sts_linear_charging.c
 *  STS Charging Strategy
 *
 *  Copyright (C) 2014-2015 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/*
 * Changelog:
 *
 * 2018-Feb:  Created. Modify driver for the battery charger control.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>

#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/charging.h>
#include <mach/mtk_charging.h>
#include <mt-plat/mtk_boot.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>



 /* ============================================================ // */
 /* define */
 /* ============================================================ // */


 /* ============================================================ // */
 /* global variable */
 /* ============================================================ // */

 /* ============================================================ // */
static void __init_charging_varaibles(void)
{
	static int init_flag;

	if (init_flag == 0) {
		init_flag = 1;
	}
}

void BATTERY_SetUSBState(int usb_state_value)
{
}

static void pchr_turn_on_charging(void)
{
	unsigned int charging_enable = KAL_TRUE;

	battery_log(BAT_LOG_FULL, "[BATTERY] pchr_turn_on_charging()!\r\n");

	if (BMT_status.bat_charging_state == CHR_ERROR) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Charger Error, turn OFF charging !\n");
		charging_enable = KAL_FALSE;
	} else if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] In meta or advanced meta mode, disable charging.\n");
		charging_enable = KAL_FALSE;
	} else {
		/*HW initialization */
		battery_log(BAT_LOG_FULL, "charging_hw_init\n");
		battery_charging_control(CHARGING_CMD_INIT, NULL);
	}

	/* enable/disable charging */
	battery_log(BAT_LOG_CRTI, "[BATTERY] pchr_turn_on_charging(), enable =%d \r\n",
		    charging_enable);
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
}

PMU_STATUS BAT_PreChargeModeAction(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] Pre-CC mode charge !!\n");

	if (BMT_status.UI_SOC >= 100) {
		BMT_status.bat_charging_state = CHR_BATFULL;
	} else if (BMT_status.UI_SOC <= 99) {
		BMT_status.bat_charging_state = CHR_CC;
	} else {
		return PMU_STATUS_FAIL;
	}

	pchr_turn_on_charging();

	return PMU_STATUS_OK;
}

PMU_STATUS BAT_ConstantCurrentModeAction(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] CC mode charge !!\n");

	if (BMT_status.UI_SOC >= 100) {
		BMT_status.bat_charging_state = CHR_BATFULL;
	} else if (BMT_status.UI_SOC <= 99) {
		BMT_status.bat_charging_state = CHR_CC;
	} else {
		return PMU_STATUS_FAIL;
	}

//	pchr_turn_on_charging();

	return PMU_STATUS_OK;
}

PMU_STATUS BAT_BatteryFullAction(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] Battery full !!\n");

	if (BMT_status.UI_SOC <= 99) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Battery Enter Re-charging!! , uisoc=(%d)\n",
			    BMT_status.UI_SOC);
		BMT_status.bat_charging_state = CHR_CC;
	}

	return PMU_STATUS_OK;
}

PMU_STATUS BAT_BatteryStatusFailAction(void)
{
	unsigned int charging_enable;

	battery_log(BAT_LOG_CRTI, "[BATTERY] BAD Battery status... Charging Stop !!\n");

	/*  Disable charger */
	charging_enable = KAL_FALSE;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	return PMU_STATUS_OK;
}

void mt_battery_charging_algorithm(void)
{
	__init_charging_varaibles();

	switch (BMT_status.bat_charging_state) {
	case CHR_PRE:
		BAT_PreChargeModeAction();
		break;

	case CHR_CC:
		BAT_ConstantCurrentModeAction();
		break;

	case CHR_BATFULL:
		BAT_BatteryFullAction();
		break;

	case CHR_ERROR:
		BAT_BatteryStatusFailAction();
		break;

	default:
		battery_log(BAT_LOG_CRTI, "[BATTERY] Unknown bat_charging_state!! \n");
		break;
	}

	battery_charging_control(CHARGING_CMD_DUMP_REGISTER, NULL);
}
