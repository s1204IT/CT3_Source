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


obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

ifeq ($(CONFIG_MTK_LC709203F_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += lc709203f.o
endif

ifeq ($(CONFIG_MTK_BQ24160_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += bq24160.o
else
ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += bq24196.o
else
ifeq ($(CONFIG_MTK_BQ24296_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += bq24296.o
else
ifeq ($(CONFIG_MTK_BQ25896_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += bq25896.o
endif
endif
endif
endif
