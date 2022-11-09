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
 * 2018-Feb:  Created. Addition of LED control.
 *
 */

#ifndef __STS_LEDS_CTRL_H__
#define __STS_LEDS_CTRL_H__

#include <linux/device.h>
#include <mt-plat/battery_common.h>

extern void leds_charging_init(struct device *dev);
extern void leds_charging_control(kal_bool err_status, kal_bool is_plugged, int batt_pct, int batt_tmpr);

#endif /* __STS_LEDS_CTRL_H__ */
