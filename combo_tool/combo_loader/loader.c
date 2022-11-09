
#include "loader.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "wmt_loader"

#define WCN_COMBO_LOADER_CHIP_ID_PROP    "persist.mtk.wcn.combo.chipid"
#define WCN_DRIVER_READY_PROP            "service.wcn.driver.ready"
#define WCN_COMBO_LOADER_DEV             "/dev/wmtdetect"
#define WMT_MODULES_PRE                  "/system/lib/modules/"
#define WMT_MODULES_SUFF                 ".ko"
#define WMT_DETECT_IOC_MAGIC                    'w'

#define COMBO_IOCTL_GET_CHIP_ID          _IOR(WMT_DETECT_IOC_MAGIC, 0, int)
#define COMBO_IOCTL_SET_CHIP_ID          _IOW(WMT_DETECT_IOC_MAGIC, 1, int)
#define COMBO_IOCTL_EXT_CHIP_DETECT      _IOR(WMT_DETECT_IOC_MAGIC, 2, int)
#define COMBO_IOCTL_GET_SOC_CHIP_ID      _IOR(WMT_DETECT_IOC_MAGIC, 3, int)
#define COMBO_IOCTL_DO_MODULE_INIT       _IOR(WMT_DETECT_IOC_MAGIC, 4, int)
#define COMBO_IOCTL_MODULE_CLEANUP       _IOR(WMT_DETECT_IOC_MAGIC, 5, int)
#define COMBO_IOCTL_EXT_CHIP_PWR_ON      _IOR(WMT_DETECT_IOC_MAGIC, 6, int)
#define COMBO_IOCTL_EXT_CHIP_PWR_OFF     _IOR(WMT_DETECT_IOC_MAGIC, 7, int)
#define COMBO_IOCTL_DO_SDIO_AUDOK        _IOR(WMT_DETECT_IOC_MAGIC, 8, int)



#define STP_WMT_MODULE_PRE_FIX           "mtk_stp_wmt"
#define STP_BT_MODULE_PRE_FIX            "mtk_stp_bt"
#define STP_GPS_MODULE_PRE_FIX           "mtk_stp_gps"
#define HIF_SDIO_MODULE_PRE_FIX          "mtk_hif_sdio"
#define STP_SDIO_MODULE_PRE_FIX          "mtk_stp_sdio"
#define STP_UART_MODULE_PRE_FIX          "mtk_stp_uart"

#define WMT_PROC_DBG_PATH                "/proc/driver/wmt_dbg"
#define WMT_PROC_AEE_PATH                "/proc/driver/wmt_aee"

static int g_loader_fd = -1;

static char g_driver_module_path[64]  = {0};
static char g_driver_module_arg[8] = "";
static int g_chipid_array[] = {
    0x6620, 0x6628, 0x6630, 0x6632, 0x6572, 0x6582, 0x6592, 0x8127,
    0x6571, 0x6752, 0x6735, 0x0321, 0x0335, 0x0337, 0x8163, 0x6580,
    0x6755, 0x0326, 0x6797, 0x0279, 0x6757, 0x0551, 0x8167
};

static int g_remove_ko_flag = 1;


//insmod
static int insmod(const char *filename, const char *args)
{
    void *module;
    unsigned int size;
    int ret = -1;
    int retry = 10;

    ALOGI("filename(%s)\n",filename);

    module = load_file(filename, &size);
    if (!module) {
        ALOGE("load file fail\n");
        return -1;
    }

    while (retry-- > 0) {
        ret = init_module(module, size, args);
        if (ret < 0) {
            ALOGE("insmod module fail(%d)\n", ret);
            usleep(30000);
        }
        else
            break;
    }

    free(module);

    return ret;
}

static int rmmod(const char *module_name)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(module_name, O_EXCL);   /*O_NONBLOCK | O_EXCL);*/
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        ALOGE("Unable to unload driver module \"%s\": %s,ret(%d)\n",
             module_name, strerror(errno), ret);
    return ret;
}

static int insmod_by_path(char *name_buf, char * module_path, char *prefix, char *postfix )
{
    int ret = -1;

    /*no need to check, upper layer API will makesure this condition fullfill*/
    strcat(name_buf, module_path);
    strcat(name_buf, prefix);
    strcat(name_buf, postfix);
    strcat(name_buf, WMT_MODULES_SUFF);

    insmod_retry:
    ret = insmod(name_buf, g_driver_module_arg);
    if (ret) {
        ALOGE("insert <%s> failed, len(%zd), ret(%d), retrying\n", name_buf, sizeof(name_buf), ret);
        /*break;*/
        usleep(800000);
        goto insmod_retry;
    } else {
        ALOGI("insert <%s> succeed\n", name_buf);
        ret = 0;
    }

    return 0;
}

/****************************************************
chip_id : for special chip handling
****************************************************/
static int insert_wmt_module_for_soc(int chip_id, char *module_path, char *name_buf, int namebuf_len)
{
    int ret = -1;
    unsigned int i = 0;
    char postfix_str[10] = {0};
    int total_len = 0;
    char *soc_modulse[] = {
        STP_WMT_MODULE_PRE_FIX,
        STP_BT_MODULE_PRE_FIX,
        STP_GPS_MODULE_PRE_FIX,
    };

    ALOGV("chip id:%x\n", chip_id);
    sprintf(postfix_str, "_%s", "soc");

    if (NULL == module_path || NULL == name_buf || 0 >= namebuf_len) {
        ALOGE("invalid parameter:modulepath(%p), namebuf(%p), namebuflen(%d)\n",
              module_path, name_buf, namebuf_len);
        return ret;
    }

    for (i = 0; i < sizeof(soc_modulse)/sizeof(soc_modulse[0]); i++) {
        total_len = sizeof (module_path) + sizeof (soc_modulse[i]) + sizeof(postfix_str) + sizeof(WMT_MODULES_SUFF);

    if (namebuf_len > total_len) {
            memset(name_buf, 0, namebuf_len);
            insmod_by_path(name_buf, module_path, soc_modulse[i], postfix_str);
        } else
            ALOGE("namebuf length(%d) too short, (%d) needed\n", namebuf_len, total_len);

    }

    return 0;
}

/****************************************************
chip_id : for special chip handling
****************************************************/
static int insert_wmt_module_for_combo(int chip_id, char *module_path, char *name_buf, int namebuf_len)
{
    int ret = -1;
    unsigned int i = 0;
    char postfix_str[10] = {0};
    int total_len = 0;

    char *combo_modulse[] = {
        HIF_SDIO_MODULE_PRE_FIX,
        STP_WMT_MODULE_PRE_FIX,
        STP_UART_MODULE_PRE_FIX,
        STP_SDIO_MODULE_PRE_FIX,
        STP_BT_MODULE_PRE_FIX,
        STP_GPS_MODULE_PRE_FIX
    };

    ALOGD("chip id:%x\n", chip_id);

    if (NULL == module_path || NULL == name_buf || 0 >= namebuf_len) {
        ALOGE("invalid parameter:modulepath(%p), namebuf(%p), namebuflen(%d)\n",
              module_path, name_buf, namebuf_len);
        return ret;
    }

    strcpy(postfix_str, "");

    for (i = 0; i < sizeof(combo_modulse)/sizeof(combo_modulse[0]); i++) {
        total_len = sizeof (module_path) + sizeof (combo_modulse[i]) + sizeof(postfix_str) + sizeof(WMT_MODULES_SUFF);
        if (namebuf_len > total_len) {
            memset(name_buf, 0, namebuf_len);
            insmod_by_path(name_buf, module_path, combo_modulse[i], postfix_str);
        } else
            ALOGE("namebuf length(%d) too short, (%d) needed\n", namebuf_len, total_len);
    }

    return 0;
}

/****************************************************
arg1 & arg2 : reserved for further usage
****************************************************/
static int insert_wmt_modules(int chip_id, int arg1, int arg2)
{
    int ret = -1;

    ALOGV("chip id:%x, arg1:%d,arg2:%d\n", chip_id, arg1, arg2);

    switch (chip_id) {
        case 0x6582:
        case 0x6572:
        case 0x6571:
        case 0x6592:
        case 0x8127:
        case 0x6752:
        case 0x6735:
        case 0x8163:
        case 0x6580:
        case 0x6755:
        case 0x6797:
        case 0x8167:
            ret = insert_wmt_module_for_soc
                (chip_id, WMT_MODULES_PRE, g_driver_module_path, sizeof(g_driver_module_path));
            break;
        case 0x6620:
        case 0x6628:
        case 0x6630:
        case 0x6632:
            ret = insert_wmt_module_for_combo
                (chip_id, WMT_MODULES_PRE, g_driver_module_path, sizeof(g_driver_module_path));
            break;
        default:
            break;
    }

    return ret;
}

static int loader_do_chipid_vaild_check(int chip_id)
{
    int ret = -1;
    unsigned char i;

    for (i = 0; i < sizeof(g_chipid_array)/sizeof(0x6630); i++) {
        if (chip_id == g_chipid_array[i]) {
            ALOGI("chipid vaild check: %d :0x%x!\n", i, chip_id);
            ret = 0;
            break;
        }
    }

    return ret;
}

static int loader_do_kernel_module_init(int loader_fd, int chip_id) {
    int ret = 0;

    if (loader_fd < 0) {
        ALOGE("invalid loaderfd: %d\n", loader_fd);
        return -1;
    }

    ret = ioctl(loader_fd, COMBO_IOCTL_MODULE_CLEANUP, chip_id);
    if (ret) {
        ALOGE("do WMT-DETECT module cleanup failed: %d\n", ret);
        return -2;
    }

    ret = ioctl(loader_fd, COMBO_IOCTL_DO_MODULE_INIT, chip_id);
    if (ret) {
        ALOGE("do kernel module init failed: %d\n", ret);
        return -3;
    }

    ALOGI("do kernel module init succeed: %d\n", ret);

    return 0;
}
static int loader_do_kernel_module_insert(int chip_id) {
    int ret = -1;
    int load_fm_result = -1;
    int load_ant_result = -1;
    int load_wlan_result = -1;

    ALOGI("rmmod mtk_wmt_detect\n");
    rmmod("mtk_wmt_detect");
    /*INSERT TARGET MODULE TO KERNEL*/
    ret = insert_wmt_modules(chip_id, 0, 0);
    /*this process should never fail*/
    if (ret) {
        ALOGE("insert wmt modules fail(%d):(%d)\n", ret, __LINE__);
        /*goto done;*/
    }

    load_fm_result = load_fm_module(chip_id);
    if (load_fm_result) {
        ALOGE("load FM modules fail(%d):(%d)\n", ret, __LINE__);
        /*continue, we cannot let this process interrupted by subsystem module load fail*/
        /*goto done;*/
    }

    load_ant_result = load_ant_module(chip_id);
    if (load_ant_result) {
        ALOGE("load ANT modules fail(%d):(%d)\n", ret, __LINE__);
        /*continue, we cannot let this process interrupted by subsystem module load fail*/
        /*goto done;*/
    }

    load_wlan_result = load_wifi_module(chip_id);
    if (load_wlan_result) {
        ALOGE("load WIFI modules fail(%d):(%d)\n", ret, __LINE__);
        /*continue, we cannot let this process interrupted by subsystem module load fail*/
        /*goto done;*/
    }

    return 0;
}
static void loader_do_combo_sdio_autok(int loader_fd, int chip_id) {
    int retry_counter = 10;
    int ret = -1;
    int no_ext_chip = -1;
    int chipid_detect = -1;
    int autok_ret = 0;

    ALOGV("chip id :%x\n", chip_id);

    /*trigger autok process, incase last autok process is interrupted by abnormal power off or battery down*/
    do {
        /*power on combo chip*/
        ret = ioctl(loader_fd, COMBO_IOCTL_EXT_CHIP_PWR_ON);
        if (0 != ret) {
            ALOGE("external combo chip power on failed\n");
            no_ext_chip = 1;
        } else {
            /*detect is there is an external combo chip, this step should not be must*/
            no_ext_chip = ioctl(loader_fd, COMBO_IOCTL_EXT_CHIP_DETECT, NULL);
        }

        ALOGI("external combo chip detected\n");
        chipid_detect = ioctl(loader_fd, COMBO_IOCTL_GET_CHIP_ID, NULL);
        ALOGI("chipid (0x%x) detected\n", chipid_detect);

        if (0 == no_ext_chip) {
            autok_ret = ioctl(loader_fd, COMBO_IOCTL_DO_SDIO_AUDOK, chipid_detect);
            ALOGE("do SDIO3.0 autok %s\n", autok_ret ? "fail" : "succeed");
        }

        ret = ioctl(loader_fd, COMBO_IOCTL_EXT_CHIP_PWR_OFF);
        ALOGI("external combo chip power off %s\n", ret ? "fail" : "succeed");

        if ((0 == no_ext_chip) && (-1 == chipid_detect)) {
            /*extenral chip detected, but no valid chipId detected, retry*/
            retry_counter--;
            ALOGE("chipId detect failed, retrying, left retryCounter:%d\n", retry_counter);
            usleep(500000);
        } else
            break;

    } while (0 < retry_counter);

}

static void loader_do_first_chip_detect(int loader_fd, int *chip) {
    int retry_counter = 1;
    int ret = -1;
    int no_ext_chip = -1;
    int chip_id = -1;
    int autok_ret = 0;
    char chipid_str[PROPERTY_VALUE_MAX] = {0};

    do {
        /*power on combo chip*/
        ret = ioctl(loader_fd, COMBO_IOCTL_EXT_CHIP_PWR_ON);
        if (0 != ret) {
            ALOGE("external combo chip power on failed\n");
            no_ext_chip = 1;
        } else {
            /*detect is there is an external combo chip*/
            no_ext_chip = ioctl(loader_fd, COMBO_IOCTL_EXT_CHIP_DETECT, NULL);
        }

        if (no_ext_chip) {     /* use soc itself */
            ALOGI("no external combo chip detected, get current soc chipid\n");
            chip_id = ioctl(loader_fd, COMBO_IOCTL_GET_SOC_CHIP_ID, NULL);
        } else {
            ALOGI("external combo chip detected\n");
            chip_id = ioctl(loader_fd, COMBO_IOCTL_GET_CHIP_ID, NULL);
        }
        ALOGI("chipid (0x%x) detected\n", chip_id);

        sprintf(chipid_str, "0x%04x", chip_id);
        ret = property_set(WCN_COMBO_LOADER_CHIP_ID_PROP, chipid_str);
        if (0 != ret) {
            ALOGE("set property(%s) to %s failed,ret:%d, errno:%d\n",
                  WCN_COMBO_LOADER_CHIP_ID_PROP, chipid_str, ret, errno);
        } else
            ALOGI("set property(%s) to %s succeed.\n", WCN_COMBO_LOADER_CHIP_ID_PROP, chipid_str);

        if (0 == no_ext_chip) {
            autok_ret = ioctl(loader_fd, COMBO_IOCTL_DO_SDIO_AUDOK, chip_id);
            ALOGI("do SDIO3.0 autok %s\n", autok_ret ? "fail" : "succeed");
            ret = ioctl(loader_fd, COMBO_IOCTL_EXT_CHIP_PWR_OFF);
            ALOGI("external combo chip power off %s\n", ret ? "fail" : "succeed");
        }

        if ((0 == no_ext_chip) && (-1 == chip_id)) {
            /*extenral chip detected, but no valid chipId detected, retry*/
            retry_counter--;
            usleep(500000);
            ALOGE("chipId detect failed, retrying, left retryCounter:%d\n", retry_counter);
        } else
            break;
    } while (0 < retry_counter);

    *chip = chip_id;
}

static int loader_do_driver_ready_set(void) {
    int ret = -1;
    char ready_str[PROPERTY_VALUE_MAX] = {0};
    ret = property_get(WCN_DRIVER_READY_PROP, ready_str, NULL);
    if ((0 >= ret) || (0 == strcmp(ready_str, "yes"))) {
        ALOGE("get property(%s) failed iRet:%d or property is %s\n",
               WCN_DRIVER_READY_PROP, ret, ready_str);
    }
    /*set it to yes anyway*/
    sprintf(ready_str, "%s", "yes");
    ret = property_set(WCN_DRIVER_READY_PROP, ready_str);
    if (0 != ret) {
        ALOGE("set property(%s) to %s failed ret:%d\n",
              WCN_DRIVER_READY_PROP, ready_str, ret);
    } else
        ALOGI("set property(%s) to %s succeed\n", WCN_DRIVER_READY_PROP, ready_str);

    return ret;
}

int main(int argc, char *argv[]) {
    int ret = -1;
    int chip_id = -1;
    int count = 0;
    char chipid_str[PROPERTY_VALUE_MAX] = {0};

    ALOGV("argc:%d,argv:%s\n", argc, argv[0]);

    do {
        g_loader_fd = open(WCN_COMBO_LOADER_DEV, O_RDWR | O_NOCTTY);
        if (g_loader_fd < 0) {
            count++;
            ALOGI("Can't open device node(%s) count(%d)\n", WCN_COMBO_LOADER_DEV, count);
            usleep(300000);
        }
        else
            break;
    }while(1);

    /*read from system property*/
    ret = property_get(WCN_COMBO_LOADER_CHIP_ID_PROP, chipid_str, NULL);
    chip_id = strtoul(chipid_str, NULL, 16);
    ALOGI("chip id from property:%d\n", chip_id);
    if ((0 != ret) && (-1 != loader_do_chipid_vaild_check(chip_id))) {
        /*valid chip_id detected*/
        ALOGI("key:(%s)-value:(%s),chipId:0x%04x,ret(%d)\n",
              WCN_COMBO_LOADER_CHIP_ID_PROP, chipid_str, chip_id, ret);
        if (0x6630 == chip_id || 0x6632 == chip_id)
            loader_do_combo_sdio_autok(g_loader_fd, chip_id);
    } else {
        /*trigger external combo chip detect and chip identification process*/
        loader_do_first_chip_detect(g_loader_fd, &chip_id);
    }

    /*set chipid to kernel*/
    ioctl(g_loader_fd, COMBO_IOCTL_SET_CHIP_ID, chip_id);

    if (g_remove_ko_flag) {
        if ((0x0321 == chip_id) || (0x0335 == chip_id) || (0x0337 == chip_id))
            chip_id = 0x6735;

        if (0x0326 == chip_id)
            chip_id = 0x6755;

        if (0x0551 == chip_id)
            chip_id = 0x6757;

        if (0x0279 == chip_id)
            chip_id = 0x6797;
        /*how to handling if not all module init fail?*/
        if (chip_id == -1) {
            ALOGE("chip id error !!(0x%x)\n", chip_id);
            return -1;
        }
        loader_do_kernel_module_init(g_loader_fd, chip_id);
        if (g_loader_fd >= 0) {
            close(g_loader_fd);
            g_loader_fd = -1;
        }
    } else {
        if (g_loader_fd >= 0) {
            close(g_loader_fd);
            g_loader_fd = -1;
        }
        loader_do_kernel_module_insert(chip_id);
    }

    if ((chown(WMT_PROC_DBG_PATH, AID_SHELL, AID_SYSTEM) == -1) ||
       (chown(WMT_PROC_AEE_PATH, AID_SHELL, AID_SYSTEM) == -1))
        ALOGE("chown wmt_dbg or wmt_aee fail:%s\n", strerror(errno));

    ret = loader_do_driver_ready_set();

    return ret;
}



