/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*
 * Copyright (C) 2017 SANYO Techno Solutions Tottori Co., Ltd.
 *
 * Changelog:
 *
 * 2018-Feb:  Support VBUS control with cherger.(USB OTG)
 *
 */

#ifdef CONFIG_MTK_BQ24196_SUPPORT
#include "../bq24196.h"
#endif

#ifdef CONFIG_MTK_BQ24297_SUPPORT
#include "../bq24297.h"
#endif

#ifdef CONFIG_MTK_BQ24296_SUPPORT
#include "../bq24296.h"
#endif

#ifdef CONFIG_MTK_NCP1851_SUPPORT
#include "../ncp1851.h"
#endif

#ifdef CONFIG_MTK_NCP1854_SUPPORT
#include "../ncp1854.h"
#endif

#ifdef CONFIG_MTK_BQ25896_SUPPORT
#include "../bq25896.h"
#endif

#include <linux/device.h>

/************* ATTENTATION ***************/
/* IF ANY NEW CHARGER IC SUPPORT IN THIS FILE, */
/* REMEMBER TO NOTIFY USB OWNER TO MODIFY OTG RELATED FILES!! */

void tbl_charger_otg_vbus(int mode)
{
	pr_debug("Power/Battery [tbl_charger_otg_vbus] mode = %d\n", mode);

	if (mode & 0xFF) {
#ifdef CONFIG_MTK_BQ24196_SUPPORT
		bq24196_set_chg_config(0x3); /*OTG*/
		bq24196_set_boost_lim(0x1);  /*1.3A on VBUS*/
		bq24196_set_en_hiz(0x0);
#endif

#ifdef CONFIG_MTK_BQ24297_SUPPORT
		bq24297_set_otg_config(0x1); /*OTG*/
		bq24297_set_boost_lim(0x1);  /*1.5A on VBUS*/
		bq24297_set_en_hiz(0x0);
#endif

#if 0
#ifdef CONFIG_MTK_BQ24296_SUPPORT
		if ( 0 == bq24296_gpio_get_pg() ) {
			bq24296_set_otg_config(0x1); /*OTG enable*/
		} else {
			bq24296_set_otg_config(0x0); /*OTG disable*/
		}
#endif
#endif

#ifdef CONFIG_MTK_NCP1851_SUPPORT
		ncp1851_set_chg_en(0x0); /*charger disable*/
		ncp1851_set_otg_en(0x1); /*otg enable*/
#endif

#ifdef CONFIG_MTK_NCP1854_SUPPORT
		ncp1854_set_chg_en(0x0); /*charger disable*/
		ncp1854_set_otg_en(0x1); /*otg enable*/
#endif

#ifdef CONFIG_MTK_BQ25896_SUPPORT
		bq25896_set_chg_en(0x0); /*charger disable*/
		bq25896_set_otg_en(0x1); /*otg enable*/
#endif
	} else {
#ifdef CONFIG_MTK_BQ24196_SUPPORT
		bq24196_set_chg_config(0x0); /*OTG & Charge disabled*/
#endif

#ifdef CONFIG_MTK_BQ24297_SUPPORT
		bq24297_set_otg_config(0x0); /*OTG & Charge disabled*/
#endif

#if 0
#ifdef CONFIG_MTK_BQ24296_SUPPORT
		bq24296_set_otg_config(0x0); /*OTG disabled*/
#endif
#endif

#ifdef CONFIG_MTK_NCP1851_SUPPORT
		ncp1851_set_otg_en(0x0);
#endif

#ifdef CONFIG_MTK_NCP1854_SUPPORT
		ncp1854_set_otg_en(0x0);
#endif

#ifdef CONFIG_MTK_BQ25896_SUPPORT
		bq25896_set_chg_en(0x0);
		bq25896_set_otg_en(0x0);
#endif
	}
};
