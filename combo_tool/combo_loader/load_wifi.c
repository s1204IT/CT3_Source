/*
*WIFI driver kernel module insmod file for wmt dynamic loader
*/
#include "loader.h"

static char g_driver_module_path[50]  = "/system/lib/modules/wlan_";
static char g_driver_module_arg[50] = "";

#ifndef WIFI_DRIVER_MODULE_ARG
#define WIFI_DRIVER_MODULE_ARG    ""
#endif

//insmod
static int insmod(const char *filename, const char *args)
{
    void *module;
    unsigned int size;
    int ret;

    module = load_file(filename, &size);
    if (!module)
        return -1;

    ret = init_module(module, size, args);

    free(module);

    return ret;
}

static int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        ALOGD("Unable to unload driver module \"%s\": %s\n",
             modname, strerror(errno));
    return ret;
}

int load_wifi_module(int chip_id)
{
    int ret = -1;

    if (chip_id == 0x6632) {
        //insert 6632 driver
        if (0 == insmod("/system/lib/modules/mtk_wmt_wifi.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod wmt wifi module\n");
        } else
            ALOGE("Fail to insmod wmt wifi module\n");

        if (0 == insmod("/system/lib/modules/wlan_mt6632.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod wlan module\n");
        }else
            ALOGE("Fail to insmod wlan module\n");
    } else if (chip_id == 0x6630) {
        //insert 6630 driver
        if (0 == insmod("/system/lib/modules/mtk_wmt_wifi.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod wmt wifi module\n");
        } else
            ALOGE("Fail to insmod wmt wifi module\n");

        if (0 == insmod("/system/lib/modules/wlan_mt6630.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod wlan module\n");
        } else
            ALOGE("Fail to insmod wlan module\n");
    } else if (chip_id == 0x6628) {
        /*insert 6628 driver */
        if (0 == insmod("/system/lib/modules/mtk_wmt_wifi.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod wmt wifi module\n");
        } else
            ALOGE("Fail to insmod wmt wifi module\n");

        if (0 == insmod("/system/lib/modules/wlan_mt6628.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod wlan module\n");
        }else
            ALOGE("Fail to insmod wlan module\n");
    } else if (chip_id == 0x6620) {
        /*insert 6620 driver*/
    } else {    /*for soc chip, same naming*/
        /*insert wmt_wifi => for temp naming*/
        if (0 == insmod("/system/lib/modules/mtk_wmt_wifi_soc.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod wmt wifi module\n");
        } else
            ALOGE("Fail to insmod wmt wifi module\n");

        /*insert wifi => for temp naming*/
        if (0 == insmod("/system/lib/modules/wlan_mt.ko", g_driver_module_arg)) {
            ret = 0;
            ALOGI("Success to insmod the %s\n", g_driver_module_path);
        } else
            ALOGE("Fail to insmod the %s\n", g_driver_module_path);
    }
    return ret;
}
