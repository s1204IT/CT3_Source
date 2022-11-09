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
 * 2018-Feb:  CCreated. Addition of LED control.
 *
 */

/**
 *	@file
 *	@brief	leds controller on battery events
 */

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/device.h>
#include <linux/hrtimer.h>

#include <mt-plat/battery_common.h>

/**
 *	@brief	threshold by celsius degree.
 */
#define	BATT_LEDS_CTRL_TMPR_TH_HIGH	45
#define	BATT_LEDS_CTRL_TMPR_TH_LOW	 0

/**
 *	@brief	threshold by battery percentage.
 */
#define	BATT_LEDS_CTRL_PCT_TH_HIGH	90
#define	BATT_LEDS_CTRL_PCT_TH_LOW	20

/**
 *	@brief	parameters of hrtimer handler.
 */
#define	BATT_LEDS_TIMER_INTERVAL	250000000		// nsec
#define	BATT_LEDS_TIMER_HDR_PHASE_NUM	4

/**
 *	@brief	leds blink table array members.
 */
typedef enum {
	BATT_LEDS_BLINK_ERROR		= 0,
	BATT_LEDS_BLINK_NETWORK,
	BATT_LEDS_BLINK_NUM
} BATT_LEDS_BLINK;

/**
 *	@brief	state table array members.
 */
typedef enum {
	BATT_LEDS_BATT_RMN_LOW		= 0,
	BATT_LEDS_BATT_RMN_MID,
	BATT_LEDS_BATT_RMN_HIGH,
	BATT_LEDS_BATT_RMN_NUM
} BATT_LEDS_BATT_RMN;

extern	int lc709203f_get_rsoc(void);
extern	int lc709203f_get_cell_temperature(void);

static	int	red_gpio,
			grn_gpio,
			is_ena_network	= FALSE;

static	struct hrtimer	 leds_timer;

/**
 *	@brief	[*][0] value is not refered.
 */
static	int	leds_blink_table[BATT_LEDS_BLINK_NUM][BATT_LEDS_TIMER_HDR_PHASE_NUM]	= {
	{ TRUE, TRUE, FALSE, FALSE},
	{ TRUE, FALSE, TRUE, FALSE}
};

/**
 *	@brief	state table array of leds. refer: specification sheet.
 */
static	int	ga_leds_sw[BATT_LEDS_BATT_RMN_NUM]	= {
	TRUE, TRUE, FALSE
};


/**
 *	@brief	read from sysfs.
 */
static ssize_t
leds_charging_show(
	struct device*				dev,
	struct device_attribute*	attr,
	char*						buf )
{
	return	sprintf( buf, "[%s] red:%2d, grn:%2d, nw:%2d.\n", __func__,
					 red_gpio, grn_gpio, is_ena_network );
}

/**
 *	@brief	write from sysfs.
 */
static ssize_t
leds_charging_store(
	struct device*				dev,
	struct device_attribute*	attr,
	const char*					buf,
	size_t						size )
{

	pr_debug(	"[%s] buf:%s.\n", __func__,
				buf );

	pr_debug(	"[%s] size:%2d.\n", __func__,
				(int)size );

	if( buf == NULL ||
		size == 0 ) {
		return	size;
	}

	if( size <= 2 ) {
		int				ret;
		unsigned int	value = 0;

		ret	= kstrtou32( buf, 0, &value);


		pr_debug(	"[%s] val:%2d.\n", __func__,
					(int)value );

		is_ena_network	= ( value == 0) ? FALSE : TRUE;
	}

	return	size;
}

//DEVICE_ATTR( batt_leds, 0664, leds_charging_show, leds_charging_store);
static struct device_attribute	dev_attr_batt_leds	= {
	.attr	= {
		.name	= __stringify( batt_leds),
		.mode	= 0666
	},
	.show	= leds_charging_show,
	.store	= leds_charging_store,
};

/**
 *	@brief	state table array of leds. refer: specification sheet.
 */
static enum hrtimer_restart
leds_charging_hrtimer_hdr(
	struct hrtimer* hrtmr )
{
	static	int	phase_cnt	= 0;
	BATT_LEDS_BLINK	pattern	= ( is_ena_network) ? BATT_LEDS_BLINK_NETWORK : BATT_LEDS_BLINK_ERROR;

	phase_cnt++;	// initial value ([0]) is set by leds_charging_control().

	pr_debug(	"[%s] cnt:%2d.\n", __func__,
				phase_cnt );

	gpio_set_value( red_gpio, leds_blink_table[pattern][phase_cnt]);

	if( phase_cnt >= (BATT_LEDS_TIMER_HDR_PHASE_NUM - 1)) {		// finish
		phase_cnt	= 0;
		return	HRTIMER_NORESTART;
	}

	hrtimer_forward_now( hrtmr, ktime_set(0, BATT_LEDS_TIMER_INTERVAL));

	return	HRTIMER_RESTART;
}

/**
 *	@brief	it's called only once from bq24296.c.
 *	@note
 *		-	it needs to be called first, before run leds_charging_control().
 */
void
leds_charging_init(
	struct device* dev )
{
	// leds: red_gpio, grn_gpio
	red_gpio	= of_get_named_gpio( dev->of_node, "red-gpio", 0);
	grn_gpio	= of_get_named_gpio( dev->of_node, "grn-gpio", 0);

	gpio_request( red_gpio, NULL);
	gpio_request( grn_gpio, NULL);

	gpio_direction_output( red_gpio, 0);
	gpio_direction_output( grn_gpio, 0);

	pr_debug(	"[%s] red_gpio:%4d, grn_gpio:%4d.\n", __func__,
				red_gpio, grn_gpio );

	hrtimer_init( &leds_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	leds_timer.function	= leds_charging_hrtimer_hdr;

	device_create_file( dev, &dev_attr_batt_leds);
}

/**
 *	@brief	called when charger plugged in and status is good.
 */
static	int
leds_charging_get_batt_rmn( int batt_pct)
{
	BATT_LEDS_BATT_RMN	batt_remain;

	/*
	 *	verify: how much is battery percentage remained.
	 */
	if( batt_pct < BATT_LEDS_CTRL_PCT_TH_LOW) {
		batt_remain	= BATT_LEDS_BATT_RMN_LOW;
	}
	else if( batt_pct >= BATT_LEDS_CTRL_PCT_TH_HIGH) {
		batt_remain	= BATT_LEDS_BATT_RMN_HIGH;
	}
	else {
		batt_remain	= BATT_LEDS_BATT_RMN_MID;
	}

	return	(int)batt_remain;
}

/**
 *	@brief	main func.
 */
void
leds_charging_control(
	kal_bool	err_status,			// !0 == error
	kal_bool	is_plugged,
	int		batt_pct,			// from 0 to 100.
	int		batt_tmpr )			// remove after the decimal point.
{
	static	int	is_red_on	= FALSE,
			is_grn_on	= FALSE;

	/*
	 *	verify: is enabled network communication now.
	 */
	if( is_ena_network) {
		is_red_on	= leds_blink_table[BATT_LEDS_BLINK_NETWORK][0];
		is_grn_on	= FALSE;

		hrtimer_start( &leds_timer, ktime_set(0, BATT_LEDS_TIMER_INTERVAL), HRTIMER_MODE_REL);
	}
	/*
	 *	verify: is plugged AC adapter or not.
	 */
	else if( ! is_plugged) {
		is_red_on	= (batt_pct < BATT_LEDS_CTRL_PCT_TH_LOW) ? TRUE : FALSE;
		is_grn_on	= FALSE;
	}
	/*
	 *	verify: is battery status good or not.
	 */
	else if( err_status) {
		is_red_on	= leds_blink_table[BATT_LEDS_BLINK_ERROR][0];
		is_grn_on	= FALSE;

		hrtimer_start( &leds_timer, ktime_set(0, BATT_LEDS_TIMER_INTERVAL), HRTIMER_MODE_REL);
	}
	/*
	 *	following from here : battery status is good.
	 */
	else {
		is_red_on	= ga_leds_sw[ leds_charging_get_batt_rmn( batt_pct)];
		is_grn_on	= TRUE;
	}

	/*
	 *	finally, set leds.
	 */
	gpio_set_value( red_gpio, is_red_on);
	gpio_set_value( grn_gpio, is_grn_on);

	pr_debug(	"[%s] red:%2d, grn:%2d, stat:%2d, plug:%2d, bpct:%3d, btmp:%3d.\n", __func__,
				is_red_on, is_grn_on, err_status, is_plugged, batt_pct, batt_tmpr );
}

MODULE_AUTHOR( "Sanyo Ltd..");
MODULE_DESCRIPTION( "for Leds");
MODULE_LICENSE( "GPL");
