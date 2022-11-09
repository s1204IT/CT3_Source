/*
 * Copyright (c) 2015-2016 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

extern struct semaphore smc_lock;
extern int forward_call_flag;
extern int irq_call_flag;
extern int fp_call_flag;
extern int keymaster_call_flag;
unsigned long teei_vfs_flag;
extern struct completion global_down_lock;
extern unsigned long teei_config_flag;

extern int get_current_cpuid(void);
