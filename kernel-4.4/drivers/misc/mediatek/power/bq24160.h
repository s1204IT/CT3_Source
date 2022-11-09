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

/*****************************************************************************
*
* Filename:
* ---------
*   bq24160.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   bq24160 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _bq24160_SW_H_
#define _bq24160_SW_H_

#ifndef BATTERY_BOOL
#define BATTERY_BOOL
typedef enum {
	KAL_FALSE = 0,
	KAL_TRUE  = 1,
} kal_bool;
#endif

#define bq24160_CON0      0x00
#define bq24160_CON1      0x01
#define bq24160_CON2      0x02
#define bq24160_CON3      0x03
#define bq24160_CON4      0x04
#define bq24160_CON5      0x05
#define bq24160_CON6      0x06
#define bq24160_CON7      0x07

/**********************************************************
  *
  *   [MASK/SHIFT]
  *
  *********************************************************/
/*CON0*/
#define CON0_TMR_RST_MASK   0x01
#define CON0_TMR_RST_SHIFT  7

#define CON0_STAT_MASK       0x07
#define CON0_STAT_SHIFT      4

#define CON0_SUPPLY_SEL_MASK   0x01
#define CON0_SUPPLY_SEL_SHIFT  3

#define CON0_FAULT_MASK       0x07
#define CON0_FAULT_SHIFT      0

/*CON1*/
#define CON1_INSTAT_MASK     0x03
#define CON1_INSTAT_SHIFT    6

#define CON1_USBSTAT_MASK     0x03
#define CON1_USBSTAT_SHIFT    4

#define CON1_OTG_LOCK_MASK     0x01
#define CON1_OTG_LOCK_SHIFT    3

#define CON1_BATSTAT_MASK     0x03
#define CON1_BATSTAT_SHIFT    1

#define CON1_EN_NOBATOP_MASK     0x01
#define CON1_EN_NOBATOP_SHIFT    0

/*CON2*/
#define CON2_RESET_MASK     0x01
#define CON2_RESET_SHIFT    7

#define CON2_IUSB_LIMIT_MASK     0x07
#define CON2_IUSB_LIMIT_SHIFT    4

#define CON2_EN_STAT_MASK     0x01
#define CON2_EN_STAT_SHIFT    3

#define CON2_TE_MASK     0x01
#define CON2_TE_SHIFT    2

#define CON2_CE_MASK     0x01
#define CON2_CE_SHIFT    1

#define CON2_HZ_MODE_MASK     0x01
#define CON2_HZ_MODE_SHIFT    0

/*CON3*/
#define CON3_VBREG_MASK     0x3f
#define CON3_VBREG_SHIFT    2

#define CON3_IINLIMIT_MASK     0x01
#define CON3_IINLIMIT_SHIFT    1

#define CON3_DDET_EN_MASK     0x01
#define CON3_DDET_EN_SHIFT    0

/*CON4*/
#define CON4_VENDOR_MASK     0x07
#define CON4_VENDOR_SHIFT    5

#define CON4_PN_MASK     0x03
#define CON4_PN_SHIFT    3

#define CON4_REVISION_MASK     0x07
#define CON4_REVISION_SHIFT    0

/*CON5*/
#define CON5_ICHRG_MASK     0x1f
#define CON5_ICHRG_SHIFT    3

#define CON5_ITERM_MASK     0x07
#define CON5_ITERM_SHIFT    0

/*CON6*/
#define CON6_MINSYS_STATUS_MASK     0x01
#define CON6_MINSYS_STATUS_SHIFT    7

#define CON6_DPM_MASK     0x01
#define CON6_DPM_SHIFT    6

#define CON6_VINDPM_USB_MASK     0x07
#define CON6_VINDPM_USB_SHIFT    3

#define CON6_VINDPM_IN_MASK     0x07
#define CON6_VINDPM_IN_SHIFT    0

/*CON7*/
#define CON7_2XTMR_EN_MASK     0x01
#define CON7_2XTMR_EN_SHIFT    7

#define CON7_TMR_MASK     0x03
#define CON7_TMR_SHIFT    5

#define CON7_TS_EN_MASK     0x01
#define CON7_TS_EN_SHIFT    3

#define CON7_TS_FAULT_MASK     0x03
#define CON7_TS_FAULT_SHIFT    1

#define CON7_LOW_CHG_MASK     0x01
#define CON7_LOW_CHG_SHIFT    0


/**********************************************************
  *
  *[Extern Function]
  *
  *********************************************************/
/*CON0----------------------------------------------------*/
extern void bq24160_set_tmr_rst(unsigned int val);
extern unsigned int bq24160_get_tmr_rst(void);
extern unsigned int bq24160_get_stat(void);
extern void bq24160_set_supply_sel(unsigned int val);
extern unsigned int bq24160_get_supply_sel(void);
extern unsigned int bq24160_get_fault(void);

/*CON1----------------------------------------------------*/
extern unsigned int bq24160_get_instat(void);
extern unsigned int bq24160_get_usbstat(void);
extern void bq24160_set_otg_lock(unsigned int val);
extern unsigned int bq24160_get_otg_lock(void);
extern unsigned int bq24160_get_batstat(void);
extern void bq24160_set_en_nobatop(unsigned int val);
extern unsigned int bq24160_get_en_nobatop(void);

/*CON2----------------------------------------------------*/
extern void bq24160_set_reset(unsigned int val);
extern void bq24160_set_iusb_limit(unsigned int val);
extern unsigned int bq24160_get_iusb_limit(void);
extern void bq24160_set_en_stat(unsigned int val);
extern unsigned int bq24160_get_en_stat(void);
extern void bq24160_set_te(unsigned int val);
extern unsigned int bq24160_get_te(void);
extern void bq24160_set_cen(unsigned int val);
extern unsigned int bq24160_get_cen(void);
extern void bq24160_set_hz_mode(unsigned int val);
extern unsigned int bq24160_get_hz_mode(void);

/*CON3----------------------------------------------------*/
extern void bq24160_set_vbreg(unsigned int val);
extern void bq24160_set_iinlimit(unsigned int val);
extern unsigned int bq24160_get_iinlimit(void);
extern void bq24160_set_ddet_en(unsigned int val);
extern unsigned int bq24160_get_ddet_en(void);

/*CON4----------------------------------------------------*/
extern unsigned int bq24160_get_vendor(void);
extern unsigned int bq24160_get_pn(void);
extern unsigned int bq24160_get_revision(void);

/*CON5----------------------------------------------------*/
extern void bq24160_set_ichrg(unsigned int val);
extern unsigned int bq24160_get_ichrg(void);
extern void bq24160_set_iterm(unsigned int val);
extern unsigned int bq24160_get_iterm(void);

/*CON6----------------------------------------------------*/
extern unsigned int bq24160_get_minsys_status(void);
extern unsigned int bq24160_get_dpm(void);
extern void bq24160_set_vindpm_usb(unsigned int val);
extern unsigned int bq24160_get_vindpm_usb(void);
extern void bq24160_set_vindpm_in(unsigned int val);
extern unsigned int bq24160_get_vindpm_in(void);

/*CON7----------------------------------------------------*/
extern void bq24160_set_2xtmr_en(unsigned int val);
extern unsigned int bq24160_get_2xtmr_en(void);
extern void bq24160_set_tmr(unsigned int val);
extern unsigned int bq24160_get_tmr(void);
extern void bq24160_set_ts_en(unsigned int val);
extern unsigned int bq24160_get_ts_en(void);
extern unsigned int bq24160_get_ts_fault(void);
extern void bq24160_set_low_chg(unsigned int val);
extern unsigned int bq24160_get_low_chg(void);

/*---------------------------------------------------------*/

extern void bq24160_dump_register(void);
extern unsigned int bq24160_read_interface(unsigned char RegNum, unsigned char *val,
	unsigned char MASK, unsigned char SHIFT);
extern unsigned int bq24160_get_charger_type(void);

extern void bq24160_charge_init(void);

#endif /* _bq24160_SW_H_*/

