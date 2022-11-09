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
 * Copyright (C) 2018 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * Changelog:
 *
 *
 */

#include <linux/types.h>
#include <mt-plat/charging.h>
#include <mt-plat/upmu_common.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/battery_meter.h>
#include <mach/mtk_battery_meter.h>
#include <mach/mtk_charging.h>
#include <mach/mtk_pmic.h>
#include "../bq24160.h"
#include <mt-plat/mtk_gpio.h>
#include <mt-plat/charging.h>
#include <mt-plat/battery_common.h>

/* ============================================================*/
/*define*/
/* ============================================================*/
#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) ARRAY_SIZE(array)


/*============================================================*/
/*global variable*/
/*============================================================*/

kal_bool charging_type_det_done = KAL_TRUE;

const unsigned int VBAT_CV_VTH[] = {
	3500000,    3520000,    3540000,    3560000,
	3580000,    3600000,    3620000,    3640000,
	3660000,    3680000,    3700000,    3720000,
	3740000,    3760000,	3780000,    3800000,
	3820000,    3840000,    3860000,    3880000,
	3900000,    3920000,    3940000,    3960000,
	3980000,    4000000,    4020000,    4040000,
	4060000,    4080000,    4100000,    4120000,
	4140000,    4160000,    4180000,    4200000,
	4220000,    4240000,    4260000,    4280000,
	4300000,    4320000,    4340000,    4360000,
	4380000,    4400000,    4420000,    4440000,
};

const unsigned int CS_VTH[] = {
	55000,
	55000 + 75 ,
	55000 + 150,
	55000 + 150 + 75,
	55000 + 300,
	55000 + 300 + 75,
	55000 + 300 + 150,
	55000 + 300 + 150 + 75,
	55000 + 600,
	55000 + 600 + 75,
	55000 + 600 + 150,
	55000 + 600 + 150 + 75,
	55000 + 600 + 300,
	55000 + 600 + 300 + 75,
	55000 + 600 + 300 + 150,
	55000 + 600 + 300 + 150 + 75,
	55000 + 1200,
	55000 + 1200 + 75,
	55000 + 1200 + 150,
	55000 + 1200 + 150 + 75,
	55000 + 1200 + 300,
	55000 + 1200 + 300 + 75,
	55000 + 1200 + 300 + 150,
	55000 + 1200 + 300 + 150 + 75,
	55000 + 1200 + 600,
	55000 + 1200 + 600 + 75,
	55000 + 1200 + 600 + 150,
	55000 + 1200 + 600 + 150 + 75,
	55000 + 1200 + 600 + 300,
	55000 + 1200 + 600 + 300 + 75,
	55000 + 1200 + 600 + 300 + 150,
	55000 + 1200 + 600 + 300 + 150 + 75,
};


/*============================================================*/
/*function prototype*/
/*============================================================*/


/*============================================================*/
/*extern variable*/
/*============================================================*/

/*============================================================*/
/*extern function*/
/*============================================================*/

/*============================================================*/
unsigned int charging_value_to_parameter(const unsigned int *parameter, const unsigned int array_size,
const unsigned int val)
{
	unsigned int temp_param;

	if (val < array_size) {
		temp_param = parameter[val];
	} else {
		battery_log(BAT_LOG_FULL, "Can't find the parameter \r\n");
		temp_param = parameter[0];
	}

	return temp_param;
}

unsigned int charging_parameter_to_value(const unsigned int *parameter, const unsigned int array_size,
const unsigned int val)
{
	unsigned int i;

	battery_log(BAT_LOG_FULL, "array_size = %d\r\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	battery_log(BAT_LOG_FULL, "NO register value match. val=%d\r\n", val);
	/*TODO: ASSERT(0);*/
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList, unsigned int number, unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = KAL_TRUE;
	else
		max_value_in_last_element = KAL_FALSE;

	if (max_value_in_last_element == KAL_TRUE) {
		/*max value in the last element*/
		for (i = (number-1); i != 0; i--) {
			if (pList[i] <= level)
				return pList[i];
		}

		battery_log(BAT_LOG_FULL, "Can't find closest level, small value first \r\n");
		return pList[0];
		/*return CHARGE_CURRENT_0_00_MA;*/
	} else {
		/*max value in the first element*/
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}

		battery_log(BAT_LOG_FULL, "Can't find closest level, large value first\r\n");
		return pList[number - 1];
		/*return CHARGE_CURRENT_0_00_MA;*/
	}
}

static unsigned int charging_hw_init(void *data)
{
	unsigned int status = STATUS_OK;

	upmu_set_rg_bc11_bb_ctrl(1);    /*BC11_BB_CTRL*/
	upmu_set_rg_bc11_rst(1);        /*BC11_RST*/

	bq24160_charge_init();

	return status;
}

static unsigned int charging_dump_register(void *data)
{
	unsigned int status = STATUS_OK;

	battery_log(BAT_LOG_FULL, "charging_dump_register\r\n");

	bq24160_dump_register();

	return status;
}

static unsigned int charging_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *)(data);

	if (enable == KAL_TRUE) {
		bq24160_set_cen(0x0); /*charger enable*/
	} else {
		bq24160_set_cen(0x1);
	}
	return status;
}

static unsigned int charging_set_cv_voltage(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int reg_val;
	unsigned int set_cv_voltage;
	unsigned int array_size;

	array_size = GETARRAYNUM(VBAT_CV_VTH);
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, *(unsigned int *) data);
	reg_val =
	    charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), set_cv_voltage);

	bq24160_set_vbreg(reg_val);

	return status;
}

static unsigned int charging_get_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int array_size;
	unsigned int reg_val;
	unsigned int ret_val;

	reg_val = bq24160_get_ichrg();

	array_size = GETARRAYNUM(CS_VTH);
	ret_val = charging_value_to_parameter(CS_VTH,array_size,reg_val);
	*(unsigned int *)data = ret_val;

	return status;
}

static unsigned int charging_set_current(void *data)
{
	unsigned int status = STATUS_OK;

	return status;
}

static unsigned int charging_set_input_current(void *data)
{
	unsigned int status = STATUS_OK;

	if(*(unsigned int *)data == CHARGE_CURRENT_2500_00_MA)
		bq24160_set_iinlimit(1);
	else
		bq24160_set_iinlimit(0);

	return status;
}

static unsigned int charging_get_charging_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int ret_val;

	ret_val = bq24160_get_stat();

	if (ret_val == 0x3 || ret_val == 0x4)
		*(unsigned int *)data = KAL_TRUE;
	else
		*(unsigned int *)data = KAL_FALSE;

	return status;
}

static unsigned int charging_reset_watch_dog_timer(void *data)
{
	unsigned int status = STATUS_OK;

	battery_log(BAT_LOG_FULL, "charging_reset_watch_dog_timer\r\n");

	bq24160_set_tmr_rst(0x1); /*Kick watchdog*/

	return status;
}

static unsigned int charging_set_hv_threshold(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_get_hv_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int ret_hvdet = 0;
	int i;

	for (i = 0; i < 2; i++) {
		ret_hvdet |= bq24160_get_batstat();
	}

	if(ret_hvdet)
		bq24160_dump_register();

	*(kal_bool *)(data) = (ret_hvdet)?(KAL_TRUE):(KAL_FALSE);

	return status;
}


static unsigned int charging_get_battery_status(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_get_charger_det_status(void *data)
{
	unsigned int status = STATUS_OK;

	if(bq24160_get_charger_type() != 0)
		*(kal_bool *)(data) = KAL_TRUE;
	else
		*(kal_bool *)(data) = KAL_FALSE;

	return status;
}

kal_bool charging_type_detection_done(void)
{
	return charging_type_det_done;
}

static unsigned int charging_get_charger_type(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int ret_val;

	ret_val = bq24160_get_charger_type();

	if(ret_val == 0x01)
		*(CHARGER_TYPE *) (data) = STANDARD_CHARGER;
	else if(ret_val == 0x02)
		*(CHARGER_TYPE *) (data) = STANDARD_HOST;

	return status;
}

static unsigned int charging_get_is_pcm_timer_trigger(void *data)
{
	unsigned int status = STATUS_OK;

	return status;
}

static unsigned int charging_set_platform_reset(void *data)
{
	unsigned int status = STATUS_OK;

	battery_log(BAT_LOG_FULL, "charging_set_platform_reset\n");
	kernel_restart("battery service reboot system");

	return status;
}

static unsigned int charging_get_platform_boot_mode(void *data)
{
	unsigned int status = STATUS_OK;

	*(unsigned int *)(data) = get_boot_mode();

	battery_log(BAT_LOG_FULL, "get_boot_mode=%d\n", get_boot_mode());

	return status;
}

static unsigned int charging_set_power_off(void *data)
{
	unsigned int status = STATUS_OK;

	battery_log(BAT_LOG_FULL, "charging_set_power_off\n");
	kernel_power_off();

	return status;
}

static unsigned int charging_get_power_source(void *data)
{
	return STATUS_UNSUPPORTED;
}

static unsigned int charging_get_csdac_full_flag(void *data)
{
	return STATUS_UNSUPPORTED;
}

static unsigned int charging_set_ta_current_pattern(void *data)
{
	return STATUS_UNSUPPORTED;
}

static unsigned int charging_set_error_state(void *data)
{
	return STATUS_UNSUPPORTED;
}

static unsigned int charging_diso_init(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_get_diso_state(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_set_vbus_ovp_en(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_set_vindpm(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_set_pwrstat_led_en(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_run_aicl(void *data)
{
	return STATUS_OK;
}

static unsigned int (*charging_func[CHARGING_CMD_NUMBER]) (void *data);

 /*
 *FUNCTION
 *		Internal_chr_control_handler
 *
 *DESCRI
 *		 This function is called to set the charger hw
 *
 *CALLS
 *
 * PARAMETERS
 *		None
 *
 *RETURNS
 *
 * GLOBALS AFFECTED
 *	   None
 */
signed int chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	signed int status;
	static signed int init = -1;

	if (init == -1) {
		init = 0;
		charging_func[CHARGING_CMD_INIT] = charging_hw_init;
		charging_func[CHARGING_CMD_DUMP_REGISTER] = charging_dump_register;
		charging_func[CHARGING_CMD_ENABLE] = charging_enable;
		charging_func[CHARGING_CMD_SET_CV_VOLTAGE] = charging_set_cv_voltage;
		charging_func[CHARGING_CMD_GET_CURRENT] = charging_get_current;
		charging_func[CHARGING_CMD_SET_CURRENT] = charging_set_current;
		charging_func[CHARGING_CMD_SET_INPUT_CURRENT] = charging_set_input_current;
		charging_func[CHARGING_CMD_GET_CHARGING_STATUS] = charging_get_charging_status;
		charging_func[CHARGING_CMD_RESET_WATCH_DOG_TIMER] = charging_reset_watch_dog_timer;
		charging_func[CHARGING_CMD_SET_HV_THRESHOLD] = charging_set_hv_threshold;
		charging_func[CHARGING_CMD_GET_HV_STATUS] = charging_get_hv_status;
		charging_func[CHARGING_CMD_GET_BATTERY_STATUS] = charging_get_battery_status;
		charging_func[CHARGING_CMD_GET_CHARGER_DET_STATUS] = charging_get_charger_det_status;
		charging_func[CHARGING_CMD_GET_CHARGER_TYPE] = charging_get_charger_type;
		charging_func[CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER] = charging_get_is_pcm_timer_trigger;
		charging_func[CHARGING_CMD_SET_PLATFORM_RESET] = charging_set_platform_reset;
		charging_func[CHARGING_CMD_GET_PLATFORM_BOOT_MODE] = charging_get_platform_boot_mode;
		charging_func[CHARGING_CMD_SET_POWER_OFF] = charging_set_power_off;
		charging_func[CHARGING_CMD_GET_POWER_SOURCE] = charging_get_power_source;
		charging_func[CHARGING_CMD_GET_CSDAC_FALL_FLAG] = charging_get_csdac_full_flag;
		charging_func[CHARGING_CMD_SET_TA_CURRENT_PATTERN] = charging_set_ta_current_pattern;
		charging_func[CHARGING_CMD_SET_ERROR_STATE] = charging_set_error_state;
		charging_func[CHARGING_CMD_DISO_INIT] = charging_diso_init;
		charging_func[CHARGING_CMD_GET_DISO_STATE] = charging_get_diso_state;
		charging_func[CHARGING_CMD_SET_VBUS_OVP_EN] = charging_set_vbus_ovp_en;
		charging_func[CHARGING_CMD_SET_VINDPM] = charging_set_vindpm;
		charging_func[CHARGING_CMD_SET_PWRSTAT_LED_EN] = charging_set_pwrstat_led_en;
		charging_func[CHARGING_CMD_RUN_AICL] = charging_run_aicl;
	}

	if (cmd < CHARGING_CMD_NUMBER && charging_func[cmd])
		status = charging_func[cmd](data);
	else {
		pr_err("Unsupported charging command:%d!\n", cmd);
		return STATUS_UNSUPPORTED;
	}
	return status;
}
