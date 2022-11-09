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
/*
 * Changelog:
 *
 * 2018-Feb:  Created. Addition of cradle connection control.
 *
 */
/**
 *	@file
 *	@brief	Input Current Controller of the charger by CRADLE_DET_B
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <mt-plat/battery_common.h>
#include <mt-plat/charging.h>

#include "sts_cradle_ctrl.h"

#define N_AR(x) (sizeof(x)/sizeof(x[0]))

typedef enum PORT_STAT {
    PORT_LO,
    PORT_HI,
    PORT_UNSTABLE,
} PORT_STAT_t;

static int det_gpio = -1;
static int adp_gpio = -1;
static PORT_STAT_t port_hist_det[2];
static PORT_STAT_t port_hist_adp[2];
static int port_index_det;
static int port_index_adp;

static int cradle_no_detect = -1;
static int adapter_no_detect = -1;

static PORT_STAT_t
average_port(PORT_STAT_t port, PORT_STAT_t hist_array[], int hist_num, int *update_index)
{
	int unstable;
	int i;
	int index;

	unstable = 0;
	for (i=0; i<hist_num; i++) {
		if (port != hist_array[i]) {
			unstable = 1;
			break;
		}
	}

	index = *update_index;
	hist_array[index] = port;
	index++;
	if (index >= hist_num)
		index = 0;
	*update_index = index;

	return (unstable)?(PORT_UNSTABLE):(port);
}


extern	void tbl_charger_otg_vbus(int mode);
static void
otg_vbus_off_check(void)
{
	if(adapter_no_detect == 0 || cradle_no_detect == 0) {
		tbl_charger_otg_vbus(0);
		battery_log(BAT_LOG_CRTI, "%s vbus off :\n", __func__ );
	}
}

static void
charger_input_control(void)
{
	static CHR_CURRENT_ENUM charger_input_current = -1;
	PORT_STAT_t cradle_det_b;
	CHR_CURRENT_ENUM input_current;

	if (det_gpio == -1)
		return;

	{
		PORT_STAT_t port;
		port = gpio_get_value(det_gpio)?(PORT_HI):(PORT_LO);
		cradle_det_b = average_port(port, port_hist_det, N_AR(port_hist_det), &port_index_det);
	}
	if (!chargin_hw_init_done)
		return;

	switch (cradle_det_b) {
	case PORT_HI: /* no cradle */
		input_current = CHARGE_CURRENT_2000_00_MA;
		break;
	case PORT_LO: /* cradle exists */
		input_current = CHARGE_CURRENT_3000_00_MA;
		break;
	default:
		return;
	}

	if (charger_input_current != input_current) {
		battery_log(BAT_LOG_CRTI, "%s :%d\n", __func__, input_current);
		battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
					 &input_current);
		charger_input_current = input_current;
	}

	if (cradle_no_detect != cradle_det_b) {
		battery_log(BAT_LOG_CRTI, "%s cradle_detect:%d\n", __func__, cradle_det_b);
		cradle_no_detect = cradle_det_b;
		otg_vbus_off_check();
	}
}
static void
adapter_input_control(void)
{
	PORT_STAT_t adapter_det_b;

	if (det_gpio == -1)
		return;

	{
		PORT_STAT_t port;
		port = gpio_get_value(adp_gpio)?(PORT_HI):(PORT_LO);
		adapter_det_b = average_port(port, port_hist_adp, N_AR(port_hist_adp), &port_index_adp);
	}
	if (!chargin_hw_init_done)
		return;

	if (adapter_no_detect != adapter_det_b) {
		battery_log(BAT_LOG_CRTI, "%s adapter_det_b:%d\n", __func__, adapter_det_b);
		adapter_no_detect = adapter_det_b;
		otg_vbus_off_check();
	}
}


int is_cradle_exist(void)
{
	return  (cradle_no_detect == 0 ? 1 : 0);
}


static void
charger_input_init(struct device_node* of_node)
{
	det_gpio = of_get_named_gpio(of_node, "det-gpio", 0);
	gpio_request(det_gpio, NULL);
	gpio_direction_input(det_gpio);
	battery_log(BAT_LOG_CRTI, "%s det_gpio:%4d\n", __func__, det_gpio);

	{
		int i;
		port_index_det = 0;
		for (i=0; i<N_AR(port_hist_det); i++) {
			PORT_STAT_t port;
			port = gpio_get_value(det_gpio)?(PORT_HI):(PORT_LO);
			(void)average_port(port, port_hist_det, N_AR(port_hist_det), &port_index_det);
		}
	}
}

static void
adapter_input_init(struct device_node* of_node)
{
	adp_gpio = of_get_named_gpio(of_node, "adp-gpio", 0);
	gpio_request(adp_gpio, NULL);
	gpio_direction_input(adp_gpio);
	battery_log(BAT_LOG_CRTI, "%s adp_gpio:%4d\n", __func__, adp_gpio);

	{
		int i;
		port_index_adp = 0;
		for (i=0; i<N_AR(port_hist_adp); i++) {
			PORT_STAT_t port;
			port = gpio_get_value(adp_gpio)?(PORT_HI):(PORT_LO);
			(void)average_port(port, port_hist_adp, N_AR(port_hist_adp), &port_index_adp);
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define CRADLE_TASK_PERIOD (100 * 1000 * 1000) /* nsecs */
static kal_bool fg_cradle_shutdown;
static kal_bool fg_cradle_thread;
static kal_bool cradle_kthread_main_flag;
static DECLARE_WAIT_QUEUE_HEAD(cradle_thread_wq);
static struct hrtimer cradle_kthread_timer;

static void
cradle_kthread_wake_up(void)
{
	cradle_kthread_main_flag = KAL_TRUE;
	wake_up(&cradle_thread_wq);
}

static enum hrtimer_restart
cradle_kthread_hrtimer_func(struct hrtimer *timer)
{
	cradle_kthread_wake_up();

	return HRTIMER_NORESTART;
}

static int
cradle_kthread_main(void *unused)
{
	ktime_t ktime = ktime_set(0, CRADLE_TASK_PERIOD);

	do {
		charger_input_control();
		adapter_input_control();
		cradle_kthread_main_flag = KAL_FALSE;
		hrtimer_start(&cradle_kthread_timer, ktime, HRTIMER_MODE_REL);
		wait_event(cradle_thread_wq, cradle_kthread_main_flag);
	} while (!fg_cradle_shutdown);

	battery_log(BAT_LOG_CRTI, "%s\n", __func__);

	fg_cradle_thread = KAL_TRUE;

	return 0;
}

int
cradle_ctrl_init(void)
{
	struct device_node *of_node;

	of_node = of_find_compatible_node(NULL, NULL, "sts,cradle");
	if (of_node == NULL) {
		battery_log(BAT_LOG_CRTI, "Failed to find device-tree node: %s\n", __func__);
		return -ENODEV;
	}
	charger_input_init(of_node);
	adapter_input_init(of_node);

	fg_cradle_shutdown = KAL_FALSE;
	fg_cradle_thread = KAL_FALSE;

	hrtimer_init(&cradle_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cradle_kthread_timer.function = cradle_kthread_hrtimer_func;
	kthread_run(cradle_kthread_main, NULL, "cradle_kthread_main");

	battery_log(BAT_LOG_CRTI, "%s\n", __func__);
	return 0;
}

void
cradle_ctrl_pause(void)
{
	hrtimer_cancel(&cradle_kthread_timer);
	battery_log(BAT_LOG_CRTI, "%s\n", __func__);
}

void
cradle_ctrl_resume(void)
{
	ktime_t ktime = ktime_set(0, CRADLE_TASK_PERIOD);

	hrtimer_start(&cradle_kthread_timer, ktime, HRTIMER_MODE_REL);
	battery_log(BAT_LOG_CRTI, "%s\n", __func__);
}

void
cradle_ctrl_shutdown(void)
{
	int count = 0;

	fg_cradle_shutdown = KAL_TRUE;
	cradle_kthread_wake_up();

	while ((!fg_cradle_thread) && (count < 5)) {
		msleep(20);
		count++;
	}
	battery_log(BAT_LOG_CRTI, "%s\n", __func__);
}

MODULE_AUTHOR("Sanyo Ltd..");
MODULE_DESCRIPTION("for controlling cradle");
MODULE_LICENSE("GPL");
