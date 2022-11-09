#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
#
# Copyright (C) 2017 SANYO Techno Solutions Tottori Co., Ltd.
#
# Changelog:
#
# 2018-Feb:  Add driver for the battery monitor.
#


MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ifeq ($(CONFIG_STS_CHARGER_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += sts_battery_meter.o sts_battery_common.o
	obj-$(CONFIG_MTK_SMART_BATTERY) += sts_linear_charging.o
else
ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	obj-$(CONFIG_MTK_SMART_BATTERY) += mtk_battery.o mtk_power_misc.o
	obj-$(CONFIG_MTK_SMART_BATTERY) += charger/
else ifeq ($(CONFIG_MTK_GAUGE_VERSION),20)
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_fg_20.o battery_common_fg_20.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) += mtk_pep_intf.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT) += mtk_pep20_intf.o
else
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter.o battery_common.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) += mtk_pep_intf.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT) += mtk_pep20_intf.o
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	#nothing
else
	ifeq ($(CONFIG_MTK_SWCHR_SUPPORT),y)
		obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
	else
		obj-$(CONFIG_MTK_SMART_BATTERY) += linear_charging.o
	endif
endif
endif
