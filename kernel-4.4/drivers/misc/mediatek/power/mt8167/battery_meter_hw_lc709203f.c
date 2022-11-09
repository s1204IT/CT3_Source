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
 * 2018-Feb     Add driver for the battery monitor.
 *
 */


#include <linux/delay.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/battery_meter_hal.h>
#include <mach/mtk_battery_meter.h>
#include "../lc709203f.h"

/*============================================================ */
/*define*/
/*============================================================ */
#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define VOLTAGE_FULL_RANGE     1800
#define ADC_PRECISE           32768  /* 15 bits*/

/*============================================================ */
/*global variable*/
/*============================================================ */
signed int chip_diff_trim_value_4_0;
signed int chip_diff_trim_value; /* unit = 0.1*/

signed int g_hw_ocv_tune_value = 8;

kal_bool g_fg_is_charging;

/*============================================================ */
/*function prototype*/
/*============================================================ */

/*============================================================ */
/*extern variable*/
/*============================================================ */

/*============================================================ */

int get_hw_ocv(void)
{
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	signed int r_val_temp = 3;

	#if defined(SWCHR_POWER_PATH)
	adc_result_reg = upmu_get_auxadc_adc_out_wakeup_swchr();
	adc_result = (adc_result_reg*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	bm_print(BM_LOG_FULL, "[oam] get_hw_ocv (swchr) : adc_result_reg=%d, adc_result=%d\n",
		adc_result_reg, adc_result);
	#else
	adc_result_reg = upmu_get_auxadc_adc_out_wakeup_pchr();
	adc_result = (adc_result_reg*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	bm_print(BM_LOG_FULL, "[oam] get_hw_ocv (pchr) : adc_result_reg=%d, adc_result=%d\n",
		adc_result_reg, adc_result);
	#endif

	adc_result += g_hw_ocv_tune_value;
	return adc_result;
}


static signed int fgauge_initialization(void *data)
{
	bm_print(BM_LOG_FULL, "%s\r\n", __FUNCTION__);

	return STATUS_OK;
}

static signed int fgauge_read_current(void *data)
{
	return STATUS_OK;
}

static signed int fgauge_read_current_sign(void *data)
{
	return STATUS_OK;
}

static signed int fgauge_read_columb(void *data)
{
	return STATUS_OK;
}

static signed int fgauge_read_rsoc(void *data)
{
	*(signed int *)data = lc709203f_get_rsoc();

	return STATUS_OK;
}

static signed int fgauge_read_rsoc_hicharge(void *data)
{
	*(signed int *)data = lc709203f_get_rsoc_hicharge();

	return STATUS_OK;
}

static signed int fgauge_hw_reset(void *data)
{
	return STATUS_OK;
}

static signed int read_adc_v_bat_sense(void *data)
{
	*(signed int *)(data) = lc709203f_get_cell_voltage();

	return STATUS_OK;
}

static signed int read_adc_v_i_sense(void *data)
{
	*(signed int *)(data) = PMIC_IMM_GetOneChannelValue(ISENSE_CHANNEL_NUMBER, 1, 1);

	return STATUS_OK;
}

static signed int read_adc_v_bat_temp(void *data)
{
	*(signed int *)(data) = lc709203f_get_cell_temperature();

	return STATUS_OK;
}

static signed int read_adc_v_charger(void *data)
{
	signed int val = 0;

	val = PMIC_IMM_GetOneChannelValue(VCHARGER_CHANNEL_NUMBER, 1, 1);
	val = (((R_CHARGER_1+R_CHARGER_2)*100*val)/R_CHARGER_2) / 100;

	*(signed int *)(data) = val;

	return STATUS_OK;
}

static signed int read_hw_ocv(void *data)
{
	*(signed int *)(data) = get_hw_ocv();

	return STATUS_OK;
}

static signed int dump_register_fgadc(void *data)
{
	lc709203f_dump_register();

	return STATUS_OK;
}

static signed int(*bm_func[BATTERY_METER_CMD_NUMBER]) (void *data);

signed int bm_ctrl_cmd(BATTERY_METER_CTRL_CMD cmd, void *data)
{
	signed int status = 0;
	static signed int init = -1;

	if (init == -1) {
		init = 0;
		bm_func[BATTERY_METER_CMD_HW_FG_INIT] = fgauge_initialization;
		bm_func[BATTERY_METER_CMD_GET_HW_FG_CURRENT] = fgauge_read_current;
		bm_func[BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN] = fgauge_read_current_sign;
		bm_func[BATTERY_METER_CMD_GET_HW_FG_CAR] = fgauge_read_columb;
		bm_func[BATTERY_METER_CMD_HW_RESET] = fgauge_hw_reset;
		bm_func[BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE] = read_adc_v_bat_sense;
		bm_func[BATTERY_METER_CMD_GET_ADC_V_I_SENSE] = read_adc_v_i_sense;
		bm_func[BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP] = read_adc_v_bat_temp;
		bm_func[BATTERY_METER_CMD_GET_ADC_V_CHARGER] = read_adc_v_charger;
		bm_func[BATTERY_METER_CMD_GET_HW_OCV] = read_hw_ocv;
		bm_func[BATTERY_METER_CMD_GET_HW_FG_RSOC] = fgauge_read_rsoc;
		bm_func[BATTERY_METER_CMD_GET_HW_FG_RSOC_HICHARGE] = fgauge_read_rsoc_hicharge;
		bm_func[BATTERY_METER_CMD_DUMP_REGISTER] = dump_register_fgadc;
//@bm_func[BATTERY_METER_CMD_SET_COLUMB_INTERRUPT] = NULL;
	}

	if (cmd < BATTERY_METER_CMD_NUMBER) {
		if (bm_func[cmd] != NULL)
			status = bm_func[cmd] (data);
	} else
		return STATUS_UNSUPPORTED;

	return status;
}
