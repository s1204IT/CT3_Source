ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/BU21025/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/base

ifeq ($(strip $(CONFIG_MTK_LCM_PHYSICAL_ROTATION)),"180")
    ccflags-y += -DLCM_PHYSICAL_ROTATION_180
else ifeq ($(strip $(CONFIG_MTK_LCM_PHYSICAL_ROTATION)),"270")
    ccflags-y += -DLCM_PHYSICAL_ROTATION_270
endif


ifeq ($(CONFIG_ARCH_MTK_PROJECT),"beg01")
    ccflags-y += -DBEG01
endif

ifeq ($(CONFIG_ARCH_MTK_PROJECT),"bee03")
    ccflags-y += -DBEE03
endif

ifeq ($(CONFIG_ARCH_MTK_PROJECT),"a04br3")
    ccflags-y += -DA04BR3
endif

obj-y += bu21025.o
