/*
 * Copyright (C) 2014
 * SANYO Techno Solutions Tottori Co., Ltd.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
/*
 * Changelog:
 *
 * 2018-Feb     Add driver for the battery monitor.
 *
 */

#ifndef __LC709203F_H__
#define __LC709203F_H__

extern void lc709203f_set_thermistor_b(u16 value);
extern void lc709203f_set_initial_rsoc(u16 value);
extern int lc709203f_get_rsoc(void);
extern int lc709203f_get_rsoc_hicharge(void);
extern int lc709203f_get_cell_temperature(void);
extern int lc709203f_get_cell_voltage(void);
extern void lc709203f_set_adjustment_pack_appli(u16 value);
extern void lc709203f_set_adjustment_pack_thermistor(u16 value);
extern void lc709203f_set_change_of_the_parameter(u16 value);
extern void lc709203f_set_alarm_low_rsoc(u16 value);
extern void lc709203f_set_alarm_low_voltage(u16 value);
extern void lc709203f_set_ic_power_mode(u16 value);
extern void lc709203f_set_status_bit(u16 value);
extern void lc709203f_dump_register(void);

#endif /* __LC70923F_H__ */
