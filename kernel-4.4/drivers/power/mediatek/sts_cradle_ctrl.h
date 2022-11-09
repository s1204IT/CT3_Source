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

#ifndef __STS_CRADLE_CTRL_H__
#define __STS_CRADLE_CTRL_H__

#include <linux/of.h>

extern int cradle_ctrl_init(void);
extern void cradle_ctrl_pause(void);
extern void cradle_ctrl_resume(void);
extern void cradle_ctrl_shutdown(void);

#endif /* __STS_CRADLE_CTRL_H__ */
