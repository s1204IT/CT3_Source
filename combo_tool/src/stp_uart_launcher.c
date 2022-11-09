
/******************************************************************************
*                         C O M P I L E R   F L A G S
*******************************************************************************
*/

/******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
*******************************************************************************
*/
#include "wmt_ioctl.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
/*#include <syslog.h>*/
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/endian.h>
#include <sys/uio.h>
#include <linux/serial.h> /* struct serial_struct  */
#include <utils/Log.h>

#include <dirent.h>
#include <cutils/properties.h>


/******************************************************************************
*                              M A C R O S
*******************************************************************************
*/

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG    "wmt_launcher"

#define  WCN_COMBO_CHIP_ID_PROP             "persist.mtk.wcn.combo.chipid"
#define  WCN_DRIVER_READY_PROP              "service.wcn.driver.ready"
#define  WCN_COMBO_COREDUMP_PROP            "service.wcn.coredump.mode"
#define  WCN_FW_DBG_LOG_PROP                "persist.mtk.wcn.fwlog.status"
#define  WCN_DYNAMIC_DUMP_PROP              "persist.mtk.wcn.dynamic.dump"
#define  WCN_COMBO_AEE_PROP                 "ro.aee.build.info"
#define  WCN_COMBO_PATCH_BUILD_VER_PROP     "persist.mtk.wcn.patch.version"
#define  WCN_ENABLE_COREDUMP_BY_PROPERTY    (0)
#define  WCN_FW_DEBUG_CLRL_RETRY_LIMIT      (5)
#define  ANDROID_BUILD_TYPE                 "ro.build.type"
#define  COREDUMP_CTRL_FILE                 "/data/coredump"

//#include "cust_mt6620_test.h" /* project custom header file */

#ifndef N_MTKSTP
#define N_MTKSTP                (15 + 1)  /* MediaTek WCN Serial Transport Protocol */
#endif
#define HCIUARTSETPROTO          _IOW('U', 200, int)
#define CUST_COMBO_WMT_DEV       "/dev/stpwmt"
#define CUST_COMBO_STP_DEV       "/dev/ttyMT2"
#define CUST_COMBO_PATCH_PATH    "/vendor/firmware"
#define CUST_COMBO_CFG_FILE      "/system/vendor/firmware/WMT.cfg"

#define CUST_BAUDRATE_DFT        (115200)
#define CUST_MULTI_PATCH         (1)
#define CUST_PATCH_SEARCH        (1)
#define PATCH_BUILD_VER_LEN      (16)
#define MAX_CMD_LEN              (NAME_MAX+1)

#define LAUNCHER_DEBUG           (0)

/******************************************************************************
*                             D A T A   T Y P E S
*******************************************************************************
*/

enum stp_modes {
    STP_MIN = 0x0,
    STP_UART_FULL = 0x1,
    STP_UART_MAND = 0x2,
    STP_BTIF_FULL = 0x3,
    STP_SDIO = 0x4,
    STP_MAX = 0x5,
};

enum stp_uart_fc {
    UART_DISABLE_FC = 0, /*NO flow control*/
    UART_MTK_SW_FC = 1,  /*MTK SW Flow Control, differs from Linux Flow Control*/
    UART_LINUX_FC = 2,   /*Linux SW Flow Control*/
    UART_HW_FC = 3,      /*HW Flow Control*/
};

enum wmt_chip_type {
    WMT_CHIP_TYPE_COMBO,
    WMT_CHIP_TYPE_SOC,
    WMT_CHIP_TYPE_INVALID
};

struct sys_property {
    const char *key;
    const char *def_value;
    char value[PROPERTY_VALUE_MAX];
};


struct stp_uart_config {
    enum stp_uart_fc fc;
    int parity;
    int stop_bit;
    int baud_rate;
};

struct stp_params_config {
    enum stp_modes stp_mode;
    int fm_mode;
    char patch_path[NAME_MAX + 1];
#if (!CUST_MULTI_PATCH && !CUST_PATCH_SEARCH)
    char patch_name[NAME_MAX + 1];
#endif
    char stp_dev[NAME_MAX + 1];
    struct stp_uart_config uart_config;
};


#if CUST_MULTI_PATCH
struct stp_patch_info {
    int download_seq;
    char address[4];
    char patch_name[NAME_MAX + 1];
};
#endif

struct chip_ant_mode_info {
    const char *cfg_item;
    char cfg_item_value[NAME_MAX + 1];
};

struct chip_mode_info {
    int chip_id;
    enum stp_modes stp_mode;
    struct chip_ant_mode_info ant_mode;
};

#ifndef WMT_PLAT_APEX
struct chip_mode_info g_chip_mode_info[] = {
    {0x6620, STP_UART_FULL, {"mt6620.defAnt", "mt6620_ant_m3.cfg"}},
    {0x6628, STP_UART_FULL, {"mt6628.defAnt", "mt6628_ant_m1.cfg"}},
    {0x6630, STP_SDIO,      {"mt6630.defAnt", "mt6630_ant_m1.cfg"}},
    {0x6632, STP_SDIO,      {"mt6632.defAnt", "mt6632_ant_m1.cfg"}},
};
#else
struct chip_mode_info g_chip_mode_info[] = {
    {0x6620, STP_UART_FULL, {"mt6620.defAnt", "WMT.cfg"}},
    {0x6628, STP_UART_FULL, {"mt6628.defAnt", "WMT.cfg"}},
    {0x6630, STP_SDIO,      {"mt6630.defAnt", "WMT.cfg"}},
    {0x6632, STP_SDIO,      {"mt6632.defAnt", "WMT.cfg"}},
};
#endif

struct fw_debug_infor {
    int log_retry; /* for polling log property retry */
    int dump_retry; /*for polling dump property retry */

    int fw_log_ctrl; /* fw log output by emi ctrl */
    int fw_dynamic_dump; /*fw coredump dynamic regin dump*/

    /* fw log output by emi state ctrl */
    char fw_log_state_orig[PROPERTY_VALUE_MAX];
    char fw_log_state_new[PROPERTY_VALUE_MAX];

    /* fw coredump dynamic regin dump state ctrl */
    char fw_dump_state_orig[PROPERTY_VALUE_MAX];
    char fw_dump_state_new[PROPERTY_VALUE_MAX];

    unsigned int bitmap;
};

struct cmd_hdr{
    char *cmd;
    int (*hdr_func)(struct stp_params_config *configs);
};

struct speed_map {
    unsigned int baud;
    speed_t      speed;
};

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/
static int cmd_hdr_baud_115k(struct stp_params_config *configs);
static int cmd_hdr_baud_921k(struct stp_params_config *configs);
static int cmd_hdr_baud_2kk(struct stp_params_config *configs);
static int cmd_hdr_baud_2_5kk(struct stp_params_config *configs);
static int cmd_hdr_baud_3kk(struct stp_params_config *configs);
static int cmd_hdr_baud_3_2kk(struct stp_params_config *configs);
static int cmd_hdr_baud_3_25kk(struct stp_params_config *configs);
static int cmd_hdr_baud_3_5kk(struct stp_params_config *configs);
static int cmd_hdr_baud_4kk(struct stp_params_config *configs);
static int cmd_hdr_stp_open(struct stp_params_config *configs);
static int cmd_hdr_stp_close(struct stp_params_config *configs);
static int cmd_hdr_stp_rst(struct stp_params_config *configs);
static int cmd_hdr_sch_patch(struct stp_params_config *configs);


/******************************************************************************
*                            P U B L I C   D A T A
*******************************************************************************
*/

/******************************************************************************
*                           P R I V A T E   D A T A
*******************************************************************************
*/
static struct speed_map g_speeds[] = {
    {115200,    B115200},
    {921600,    B921600},
    {1000000,    B1000000},
    {1152000,    B1152000},
    {2000000,    B2000000},
    {2500000,    B2500000},
    {3000000,    B3000000},
    {3500000,    B3500000},
    {4000000,    B4000000},
};

struct cmd_hdr g_cmd_hdr_table[] = {
    { "baud_115200_0", cmd_hdr_baud_115k},
    { "baud_921600_0", cmd_hdr_baud_921k},    
    { "baud_2000000_0", cmd_hdr_baud_2kk},
    { "baud_2500000_0", cmd_hdr_baud_2_5kk},    
    { "baud_3000000_0", cmd_hdr_baud_3kk},
    //{ "baud_3200000_0", cmd_hdr_baud_3_2kk},
    //{ "baud_3250000_0", cmd_hdr_baud_3_25kk},   
    { "baud_3500000_0", cmd_hdr_baud_3_5kk},
    { "baud_4000000_0", cmd_hdr_baud_4kk},
    { "open_stp", cmd_hdr_stp_open},
    { "close_stp", cmd_hdr_stp_close},
    { "rst_stp", cmd_hdr_stp_rst},
    { "srh_patch", cmd_hdr_sch_patch},
};

static volatile sig_atomic_t __io_canceled = 0;
static char g_wmt_cfg_name[NAME_MAX + 1] = {0};
static int g_wmt_fd = -1;
static int g_tty_fd = -1;
static char g_cmd_str[MAX_CMD_LEN]= {0};
static char g_resp_str[MAX_CMD_LEN]= {0};
static struct stp_params_config g_stp_param_config;

pthread_t g_thread_handle = -1;
static struct fw_debug_infor g_fw_debug_infor;

#if CUST_MULTI_PATCH
static char g_patch_prefix[16];
#endif
/******************************************************************************
*                              F U N C T I O N S
*******************************************************************************
*/

static speed_t get_speed(int baud_rate) {
    unsigned int idx;
    for (idx = 0; idx < sizeof(g_speeds)/sizeof(g_speeds[0]); idx++) {
        if (baud_rate == (int)g_speeds[idx].baud) {
            return g_speeds[idx].speed;
        }
    }
    return CBAUDEX;
}

static int set_speed(int fd, struct termios *ti, int speed) {
    struct serial_struct ss;
    int baudenum = get_speed(speed);

    if (speed != CBAUDEX) {
        /*printf("%s: standard baudrate: %d -> 0x%08x\n", __FUNCTION__, speed, baudenum);*/
        if ((ioctl(fd, TIOCGSERIAL, &ss)) < 0) {
            ALOGE("%s: BAUD: error to get the serial_struct info:%s\n", __FUNCTION__, strerror(errno));
            return -1;
        }
        ss.flags &= ~ASYNC_SPD_CUST;
#if defined(SERIAL_STRUCT_EXT) /*modified in serial_struct.h*/
        memset(ss.reserved, 0x00, sizeof(ss.reserved));
#endif
        ss.flags |= (1 << 13);    /*set UPFLOWLATENCY flat to tty, or serial_core will reset tty->low_latency to 0*/
        /*set standard buadrate setting*/
        if ((ioctl(fd, TIOCSSERIAL, &ss)) < 0) {
            ALOGE("%s: BAUD: error to set serial_struct:%s\n", __FUNCTION__, strerror(errno));
            return -2;
        }
        cfsetospeed(ti, baudenum);
        cfsetispeed(ti, baudenum);
        return tcsetattr(fd, TCSANOW, ti);
    }
    else {
        ALOGE("%s: unsupported non-standard baudrate: %d -> 0x%08x\n", __FUNCTION__, speed, baudenum);
        return -3;
    }
}
/* Used as host uart param setup callback */
static int launcher_setup_uart_param(int com_port, struct stp_uart_config *uart_config)
{
    struct termios ti;
    int  fd;

    if (!uart_config) {
        ALOGE("Invalid stpUartConfig");
        return -2;
    }
    
    ALOGI("setup_uart_param %d %d\n", uart_config->baud_rate, uart_config->fc);

    fd = com_port;
    if (fd < 0) {
        ALOGE("Invalid serial port");
        return -2;
    }

    tcflush(fd, TCIOFLUSH);

    if (tcgetattr(fd, &ti) < 0) {
        ALOGE("Can't get port settings");
        return -3;
    }

    cfmakeraw(&ti);

    ALOGI("ti.c_cflag = 0x%08x\n", ti.c_cflag);
    ti.c_cflag |= CLOCAL;
    ALOGI("CLOCAL = 0x%x\n", CLOCAL);
    ALOGI("(ori)ti.c_iflag = 0x%08x\n", ti.c_iflag);
    ALOGI("(ori)ti.c_cflag = 0x%08x\n", ti.c_cflag);
    ALOGI("stpuartconfig->fc= %d (0:none,sw,hw,linux)\n", uart_config->fc);
     
    if (uart_config->fc == UART_DISABLE_FC) {
        ti.c_cflag &= ~CRTSCTS;
        ti.c_iflag &= ~(0x80000000);
    } else if (uart_config->fc == UART_MTK_SW_FC) {
        ti.c_cflag &= ~CRTSCTS;
        ti.c_iflag |= 0x80000000;   /*MTK Software FC*/
    } else if (uart_config->fc == UART_HW_FC) {
        ti.c_cflag |= CRTSCTS;      /*RTS, CTS Enable*/
        ti.c_iflag &= ~(0x80000000);
    } else if (uart_config->fc == UART_LINUX_FC) {
        ti.c_iflag |= (IXON | IXOFF | IXANY); /*Linux Software FC*/
        ti.c_cflag &= ~CRTSCTS;
        ti.c_iflag &= ~(0x80000000);
    }else {
        ti.c_cflag &= ~CRTSCTS;
        ti.c_iflag &= ~(0x80000000);
    }
    
    ALOGI("c_c CRTSCTS = 0x%16x\n", CRTSCTS);
    ALOGI("c_i IXON = 0x%08x\n", IXON);
    ALOGI("c_i IXOFF = 0x%08x\n", IXOFF);
    ALOGI("c_i IXANY = 0x%08x\n", IXANY);
    ALOGI("(aft)ti.c_iflag = 0x%08x\n", ti.c_iflag);
    ALOGI("(aft)ti.c_cflag = 0x%08x\n\n", ti.c_cflag);

    if (tcsetattr(fd, TCSANOW, &ti) < 0) {
        ALOGE("Can't set port settings");
        return -4;
    }

    /* Set baudrate */
    if (set_speed(fd, &ti, uart_config->baud_rate) < 0) {
        ALOGE("Can't set initial baud rate");
        return -5;
    }

    tcflush(fd, TCIOFLUSH);

    return 0;
}
static int cmd_hdr_baud_115k(struct stp_params_config *config)
{
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 115200;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_921k(struct stp_params_config *config)
{
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 921600;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_2kk(struct stp_params_config *config) {
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 2000000;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_2_5kk(struct stp_params_config *config)
{
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 2500000;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_3kk(struct stp_params_config *config) {
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 3000000;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_3_2kk(struct stp_params_config *config) {
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 3200000;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_3_25kk(struct stp_params_config *config) {
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 3250000;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_3_5kk(struct stp_params_config *config) {
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 3500000;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_baud_4kk(struct stp_params_config *config) {
    struct stp_uart_config *uart_configs = &config->uart_config;
    uart_configs->baud_rate = 4000000;
    return (g_tty_fd != -1) ? launcher_setup_uart_param(g_tty_fd, uart_configs) : -1;
}

static int cmd_hdr_stp_open(struct stp_params_config *config) {
    int ld;

    ALOGV("%p\n", config);
    if ((STP_UART_FULL == g_stp_param_config.stp_mode) && (-1 == g_tty_fd)) {
        g_tty_fd = open(g_stp_param_config.stp_dev, O_RDWR | O_NOCTTY);
        if (g_tty_fd < 0) {
            fprintf(stderr, "Can't open serial port %s\n", g_stp_param_config.stp_dev);
            return -2;
        }
        ALOGI("real_tty(%s) opened(%d)\n", g_stp_param_config.stp_dev, g_tty_fd);

        /* Set TTY to N_MTKSTP line discipline */
        ld = N_MTKSTP;
        if (ioctl(g_tty_fd, TIOCSETD, &ld) < 0) {
            fprintf(stderr, "Can't set ldisc to N_MTKSTP\n");
            return -3;
        }

         //printf("Set tty->low_latency\n");
         if (ioctl(g_tty_fd, HCIUARTSETPROTO, 0) < 0) {
            ALOGE("Can't set HCIUARTSETPROTO\n");
            return -4;
        }
        return 0;
    }
    else {
        fprintf(stderr, "stp_open fail: stp_mode(%d) real_tty_fd(%d) \n",
                g_stp_param_config.stp_mode, g_tty_fd);
        return -1;
    }
}

static int cmd_hdr_stp_close(struct stp_params_config *config) {
    int ld;

    ALOGV("%p\n", config);
    if ((STP_UART_FULL == g_stp_param_config.stp_mode) && (0 <= g_tty_fd)) {
        /* Restore TTY line discipline */
        ld = N_TTY;
        if (ioctl(g_tty_fd, TIOCSETD, &ld) < 0) {
            ALOGE("Can't restore line discipline");
            return -2;
        }

        close(g_tty_fd);
        g_tty_fd = -1;
        return 0;
    } else if (g_tty_fd == -1)
        return 0;
    else {
        fprintf(stderr, "stp_close fail: stp_mode(%d) real_tty_fd(%d) \n", g_stp_param_config.stp_mode, g_tty_fd);
        return -1;
    }
}

static int cmd_hdr_stp_rst(struct stp_params_config *config) {
    int ret = 0;
    /*this step fail?*/
    ret = cmd_hdr_stp_close(config);
    /*here, launcher is close state*/
    ret = cmd_hdr_stp_open(config);
    return ret;
}

static void launcher_set_patch_version(int fd)
{
    int ret = 0;
    struct sys_property patch_ver_prop;
    int bytes;
    char patch_build_ver[PATCH_BUILD_VER_LEN] = {0};

    /*4.2. read patch header and check if metch with MAJOR+MINOR number in fw version */
    bytes = read(fd, patch_build_ver, PATCH_BUILD_VER_LEN);
    if (-1 == bytes)
        ALOGE("read patch_build_ver failed!\n");

    patch_build_ver[PATCH_BUILD_VER_LEN - 1] = '\0';

    patch_ver_prop.key = WCN_COMBO_PATCH_BUILD_VER_PROP;
    sprintf(patch_ver_prop.value, "%s", patch_build_ver);

    ret = property_set(patch_ver_prop.key, patch_ver_prop.value);
    if (0 != ret)
        ALOGE("set property(%s) to %s failed,ret:%d, errno:%d\n",
               patch_ver_prop.key, patch_ver_prop.value, ret, errno);
    else
        ALOGI("set property(%s) to %s succeed.\n",
               patch_ver_prop.key, patch_ver_prop.value);
}


#if CUST_MULTI_PATCH
static int launcher_get_patch_prefix(int chip_id, char **patch_prefix) {
    /* use malloc for alloc memory is better*/
    int chip = chip_id;

    if (!patch_prefix) {
        ALOGE("invalid pointer\n");
        return -1;
    }

    if ((0x0321 == chip) || (0x0335 == chip) || (0x0337 == chip)) {
        chip = 0x6735;
        ALOGI("for denali chipid convert\n");
    }
    if (0x0326 == chip) {
        chip = 0x6755;
        ALOGI("for jade chipid convert\n");
    }
    if (0x0279 == chip) {
        chip = 0x6797;
        ALOGI("for everest chipid convert\n");
    }
    if (0x0551 == chip) {
        chip = 0x6757;
        ALOGI("for olympus chipid convert\n");
    }

    memset(g_patch_prefix, '\0', sizeof(g_patch_prefix));

    switch (chip) {
        case 0x6572:
        case 0x6582:
        case 0x6592:
            strncpy(g_patch_prefix, "ROMv1", strlen("ROMv1"));
        break;
        case 0x8127:
        case 0x6571:
            strncpy(g_patch_prefix, "ROMv2", strlen("ROMv2"));
        break;
        case 0x6755:
        case 0x6752:
        case 0x8163:
        case 0x7623:
        case 0x6735:
        case 0x6580:
        case 0x6757:
        case 0x8167:
            strncpy(g_patch_prefix, "ROMv2_lm", strlen("ROMv2_lm"));
        break;
        case 0x6797:
            strncpy(g_patch_prefix, "ROMv3", strlen("ROMv3"));
        break;
        default:
            strncpy(g_patch_prefix, "mt", strlen("mt"));
            sprintf(g_patch_prefix + strlen("mt"), "%04x", chip);
        break;
    }
    strcat(g_patch_prefix, "_patch");

    ALOGI("patch name pre-fix:%s\n", g_patch_prefix);

    *patch_prefix = g_patch_prefix;
    return 0;
}

static int launcher_get_patch_version(int patch_fd, unsigned int *patch_ver)
{
    int bytes = -1;
    int ret = 0;

    bytes = read(patch_fd, ((char *)patch_ver) + 1, 1);
    if (-1 == bytes) {
        ALOGE("read patch_ver failed!\n");
        ret = -1;
    }
    bytes = read(patch_fd, ((char *)patch_ver), 1);
    if (-1 == bytes) {
        ALOGE("read patch_ver failed!\n");
        ret = -2;
    }

    return ret;
}

static int launcher_set_patch_info(int fd, char *patch_info, char *full_name, int is_first) {
    int patch_num = -1;
    int patch_seq = -1;
    struct stp_patch_info pi;

    if (!patch_info) {
        ALOGE("invalid patch infor!\n");
        return -1;
    }
    if (!full_name) {
        ALOGE("invalid patch full name!\n");
        return -2;
    }

    if (is_first) {
        patch_num = (patch_info[0] & 0xF0) >> 4;
        ALOGI("patch num = [%d]\n", patch_num);
        ioctl(fd, WMT_IOCTL_SET_PATCH_NUM, patch_num);
    }

    patch_seq = (patch_info[0] & 0x0F);
    ALOGI("patch seq = [%d]\n", patch_seq);
    pi.download_seq = patch_seq;
    memcpy(pi.address, patch_info, sizeof(pi.address));
    pi.address[0] = 0x00;
    strncpy(pi.patch_name, full_name, sizeof(pi.patch_name) - 1);
    pi.patch_name[sizeof(pi.patch_name) - 1] = '\0';

    ioctl(fd, WMT_IOCTL_SET_PATCH_INFO, &pi);

    return 0;
}

static int cmd_hdr_sch_patch(struct stp_params_config *config) {
    int chip_id = 0;
    int fw_version = 0;
    char *patch_prefix = NULL;
    char patch_full_name[256] = {0};
    char patchName[128] = {0};
    unsigned int patch_ver = 0;
    DIR *dir = NULL;
    int patch_fd = -1;
    int ret = 0;
    int bytes;
    char patch_info[8] = {0};
    unsigned int is_first = 1;
    struct dirent* dirent = NULL;

    if (g_wmt_fd <= 0) {
        ALOGE("file descriptor is not valid\n");
        return -1;
    }
    /*1. ioctl to get CHIP ID*/
    chip_id = ioctl(g_wmt_fd, WMT_IOCTL_GET_CHIP_INFO, 0);
    if (launcher_get_patch_prefix(chip_id, &patch_prefix)) {
        ALOGE("launcher_get_patch_prefix fail\n");
        return -2;
    }

    /*2. ioctl to get FIRMWARE VERSION*/
    fw_version = ioctl(g_wmt_fd, WMT_IOCTL_GET_CHIP_INFO, 2);
    ALOGI("fw_version:0x%04x\n", fw_version);
    /*3. open directory patch located*/
    strncpy(config->patch_path, CUST_COMBO_PATCH_PATH, strlen(CUST_COMBO_PATCH_PATH));
    config->patch_path[strlen(CUST_COMBO_PATCH_PATH) + 1] = '\0';

    dir = opendir(config->patch_path);
    if (NULL == dir) {
        ALOGE("patch path cannot be opened");
        return -3;
    }
    while (NULL != (dirent = readdir(dir))) {
        patch_ver = 0;
        if (0 == (strncmp(dirent->d_name, patch_prefix, strlen(patch_prefix)))) {
            /*4.1. search patch name begined with chip_name*/
            strcpy(patch_full_name, config->patch_path);
            /*robust, if input patch is /etc/firmwre/ no issue should be happened.*/
            strcat(patch_full_name, "/");
            strcat(patch_full_name, dirent->d_name);
            ALOGI("%s\n", patch_full_name);
            strcpy (patchName, dirent->d_name);
            /*4.1. search patch name mt[CHIP ID]xxx.bin*/
            if (0 <= (patch_fd = (open(patch_full_name, O_RDONLY)))) {
                launcher_set_patch_version(patch_fd);

                if (-1 != lseek(patch_fd, 22, SEEK_SET)) {
                    if (launcher_get_patch_version(patch_fd, &patch_ver)) {
                        ALOGE("read patch_ver failed!\n");
                        ret = -4;
                        goto read_fail;
                    }
                      /*print hardware version information in patch*/
                    ALOGI("fw Ver in patch: 0x%04x\n", patch_ver);
                    if (0 == ((patch_ver ^ fw_version) & 0x00ff)) {
                        memset(patch_info, 0, sizeof(patch_info));
                        bytes = read(patch_fd, patch_info, 4);
                        if (-1 == bytes) {
                            ret = -5;
                            ALOGE("read patch_info failed!\n");
                            goto read_fail;
                        }
                        patch_info[4] = '\0';
                        ALOGI("read patch info:0x%02x,0x%02x,0x%02x,0x%02x\n",
                               patch_info[0], patch_info[1], patch_info[2], patch_info[3]);
                        if (launcher_set_patch_info(g_wmt_fd, patch_info, patchName, is_first)) {
                            ALOGE("launcher_set_patch_info fail\n");
                            ret = -6;
                            goto read_fail;
                        }
                        is_first = 0;
                    }
                } else
                    ALOGE("seek failed\n");
read_fail:
                close(patch_fd);
                patch_fd = -1;
            } else
                ALOGE("open patch file(%s) failed\n", patchName);
        }
    }
    /*5. return value*/
    closedir(dir);
    dir = NULL;

    return ret;
}
#else
static int cmd_hdr_sch_patch(struct stp_params_config *config) {
    int chip_id = 0;
    int fw_version = 0;
    char chip_name[16] = {0};
    char patch_full_name[256] = {0};
    unsigned int patch_ver = 0;
    DIR *dir = NULL;
    int patch_fd = -1;
    struct dirent* dirent = NULL;
    int ret = -1;

    if (g_wmt_fd <= 0) {
        ALOGE("file descriptor is not valid\n");
        return -1;
    }

    /*1. ioctl to get CHIP ID*/
    chip_id = ioctl(g_wmt_fd, WMT_IOCTL_GET_CHIP_INFO, 0);
    strcpy(chip_name, "mt");
    sprintf(chip_name + strlen(chip_name), "%04x", chip_id);
    strcat(chip_name, "_patch");
    ALOGI("patch name pre-fix:%s\n", chip_name);

    /*2. ioctl to get FIRMWARE VERSION*/
    fw_version = ioctl(g_wmt_fd, WMT_IOCTL_GET_CHIP_INFO, 2);
    ALOGI("fwVersion:0x%04x\n", fw_version);

    /*3. open directory patch located*/
    strncpy(config->patch_path, CUST_COMBO_PATCH_PATH, strlen(CUST_COMBO_PATCH_PATH));
    config->patch_path[strlen(CUST_COMBO_PATCH_PATH) + 1] = '\0';

    dir = opendir(config->patch_path);
    if (NULL == dir) {
        ALOGE("patch path cannot be opened");
        ret = -2;
        return ret;
    }
    while (NULL != (dirent = readdir(dir))) {
        patch_ver = 0;
        if (0 == (strncmp(dirent->d_name, chip_name, strlen(chip_name)))) {
            /*4.1. search patch name begined with chipName*/
            strcpy(patch_full_name, config->patch_path);
            /*robust, if input patch is /etc/firmwre/ no issue should be happened.*/
            strcat(patch_full_name, "/");
            strcat(patch_full_name, dirent->d_name);
            ALOGI("%s\n", patch_full_name);
            /*4.1. search patch name mt[CHIP ID]xxx.bin*/
            if (0 < (patch_fd = (open(patch_full_name, O_RDONLY)))) {
              /*4.2. read patch header and check if metch with MAJOR+MINOR number in fw version */
                if (-1 != lseek(patch_fd, 22, SEEK_SET)) {
                    read(patch_fd, ((char *)&patch_ver) + 1, 1);
                    read(patch_fd, ((char *)&patch_ver), 1);
                    /*print firmware version information in patch*/
                    ALOGI("fw Ver in patch: 0x%04x\n", patch_ver);
                    if (0 == ((patch_ver ^ fw_version) & 0x00ff)) {
                        ioctl(g_wmt_fd, WMT_IOCTL_SET_PATCH_NAME, patch_full_name);
                        ALOGI("fw Ver in patch matches with firmware version\n");
                        ret = 0;
                        close(patch_fd);
                        break;
                    }
                } else
                    ALOGE("seek failed\n");

                close(patch_fd);
                patch_fd = -1;
            } else
                ALOGE("open patch file(%s) failed\n", patch_full_name);
        }
    }
    /*5. return value*/
    closedir(dir);
    dir = NULL;

    return ret;
}
#endif
/*
ret 0: success
ret 1: cmd not found
ret -x: handler return value
*/
static int launcher_handle_cmd(struct stp_params_config *stp_param_config, char *cmd, int len) {
    int ret = 1;
    int i;
    int cmd_len;
    
    for (i = 0; i < (int)(sizeof(g_cmd_hdr_table)/sizeof(g_cmd_hdr_table[0])); ++i) {
        cmd_len = (int)strlen(g_cmd_hdr_table[i].cmd);
        if (!strncmp(g_cmd_hdr_table[i].cmd, cmd, (len < cmd_len) ? len : cmd_len)) {
            ret = (*g_cmd_hdr_table[i].hdr_func)(stp_param_config);
        }
    }

    return ret;
}

static void launcher_display_usage(void)
{
    unsigned int index = 0;

    char * usage[] = {
        "MTK WCN combo tool set, version 1.0-release",
        "Usage: wmt_launcher [-m mode] -p patchfolderpath [-d uartdevicenode] [-b baudrate] [-c uartflowcontrol]",
        "    -m (BT/GPS/FM common interface mode selection, optional)",
        "        -1: UART mode (common interface: UART)",
        "        -3: BTIF mode (common interface: BTIF)",
        "        -4: SDIO mode (common interface: SDIO)",
        "    -p (MTK WCN chip firmware patch location, must)",
        "        -e.g. /system/etc/firmware",
        "    -n (MTK WCN chip firmware patch name, option)",
        "        -e.g. /system/etc/firmware/firmware/mt6628_patch_hdr.bin",
        "    if UART mode is set, you need config baud rate and tty device:",
        "    -b (Baudrate set when BT/GPS/FM runs under UART mode, no needed under SDIO mode)",
        "        -115200/921600/2000000/2500000/3000000/3500000/4000000",
        "    -d (UART device node, when under UART mode, no needed under SDIO mode)",
        "        -e.g. /dev/ttyMT1, /dev/ttyMT2, /dev/ttyHS2, etc.",
        "    -c (UART flowcontrol set)",
        "        -0, no flowcontrol default value, please donot modify this parameter",
        "e.g. wmt_launcher -m 3 -p /system/etc/firmware/",
        "e.g. wmt_launcher -m 1 -n /system/etc/firmware/mt6628_patch_hdr.bin",
        "e.g. wmt_launcher -m 4 -d /dev/ttyMT2 -b 4000000 -n /system/etc/firmware/mt6628_patch_hdr.bin",
    };

    for (index = 0; index < sizeof(usage) / sizeof(usage[0]); index++ )
        ALOGI("%s\n", usage[index]);

    exit(EXIT_FAILURE);
}


static int launcher_stp_param_valid_check(struct stp_params_config *stp_param_configs, enum wmt_chip_type chip_type)
{
    int ret = 0;

    /*fix me, check condition should be more reasonable*/
    if ('\0' == stp_param_configs->patch_path[0]) {
        ALOGE("MCU patch path not config,use /system/etc/firmware\n");
        strncpy(stp_param_configs->patch_path, "/system/etc/firmware", strlen("/system/etc/firmware"));
        stp_param_configs->patch_path[strlen("/system/etc/firmware") + 1] = '\0';
        ret = -1;
    }

#if (!CUST_MULTI_PATCH && !CUST_PATCH_SEARCH)
    if ('\0' == stp_param_configs->patch_name[0]) {
        ALOGE("MCU patch path not config,you need config it yourself\n");
        ret = -2;
    }
#endif
    if (stp_param_configs->stp_mode != STP_SDIO &&
        stp_param_configs->stp_mode != STP_UART_MAND &&
        stp_param_configs->stp_mode != STP_UART_FULL &&
        stp_param_configs->stp_mode != STP_BTIF_FULL) {
        if (WMT_CHIP_TYPE_COMBO == chip_type) {
            ALOGE("Stp Mode is not set, common interface use default: SDIO Mode");
            stp_param_configs->stp_mode = STP_SDIO;
        } else {
            ALOGE("Stp Mode is not set, common interface use default: BTIF Mode");
            stp_param_configs->stp_mode = STP_BTIF_FULL;
        }
        ret = -3;
    }

    if (stp_param_configs->stp_mode == STP_UART_MAND || stp_param_configs->stp_mode == STP_UART_FULL) {
        ALOGI ("Common Interface: UART mode\n");
        if ('\0' == stp_param_configs->stp_dev[0]) {
            strncpy(stp_param_configs->stp_dev, CUST_COMBO_STP_DEV, strlen(CUST_COMBO_STP_DEV));
            ALOGI("no uart device input, use default: %s\n", stp_param_configs->stp_dev);
            ret = -4;
        }
        if (stp_param_configs->uart_config.baud_rate < 0) {
            /* FixMe:Chaozhong, add baudrate validation check */
            stp_param_configs->uart_config.baud_rate = 4000000;
            ALOGI("no baudrate input, use default: %d\n", stp_param_configs->uart_config.baud_rate);
            ret = -5;
        }
    }
    return ret;
}

static int launcher_parser_wmt_cfg(char *item)
{
    int index = 0;
    int length = 0;
    char *str = NULL;
    char *key = NULL;
    char *value = NULL;
    int max_index  = sizeof (g_chip_mode_info) / sizeof (g_chip_mode_info[0]);

    if (NULL == item) {
        ALOGE("Warning:item is NULL\n");
        return -1;
    }
    /*all item must be start with mt66xx*/
    str = strstr(item, "m");
    if (NULL == str) {
        ALOGE("Warning:no string start with 'm' found in %s\n", item);
        return -2;
    }
        
    for (index = 0; index < max_index; index++) {
        key = (char*)g_chip_mode_info[index].ant_mode.cfg_item;
        
        if (0 == strncasecmp(str, key, strlen(key))) {
            str = strstr(str, "=");
            if (NULL == str) {
                ALOGE("Warning:no '=' found in %s\n", str);
                return -3;
            }
            str = strstr(str, "m");
            if (NULL == str) {
                ALOGE("Warning:no 'm' found in %s\n", str);
                return -4;
            }

            while (((*str) == ' ') || ((*str) == '\t') || ((*str) == '\n')) {
                if (str >= item + strlen(item))
                    break;
                str++;
            }
            value = str;

            while (((*str) != ' ') && ((*str) != '\t') && ((*str) != '\0') && ((*str) != '\n') && ((*str) != '\r')) {
                if (str >= item + strlen(item))
                    break;
                str++;
            }
            *str = '\0';
            length = sizeof(g_chip_mode_info[index].ant_mode.cfg_item_value);
            strncpy(g_chip_mode_info[index].ant_mode.cfg_item_value, value, length - 1);
            g_chip_mode_info[index].ant_mode.cfg_item_value[length - 1] = '\0';
            ALOGI("Info:key:%s value:%s\n", key, g_chip_mode_info[index].ant_mode.cfg_item_value);
            break;
        }
    }

    return 0;
}

static int launcher_set_coredump_flag(void)
{
    struct sys_property build_type;
    struct sys_property coredump_mode;
    struct sys_property aee_property;
    int coredump_enable = 0;
    int ret = -1;

    build_type.key = ANDROID_BUILD_TYPE;
    build_type.def_value = NULL;
    coredump_mode.key = WCN_COMBO_COREDUMP_PROP;
    coredump_mode.def_value = NULL;
    aee_property.key = WCN_COMBO_AEE_PROP;
    aee_property.def_value = NULL;

    if (g_wmt_fd < 0) {
        ALOGE("%s:invalid wmt fd\n", __func__);
        return -1;
    }
#if WCN_ENABLE_COREDUMP_BY_PROPERTY
    coredump_enable = 2;
#else
    //read from system property
    ret = property_get(build_type.key, build_type.value, build_type.def_value);
    if (0 != ret) {
        ALOGI("key:(%s)-value:(%s)\n", build_type.key, build_type.value);
        if (0 == strcmp(build_type.value, "user")) {
            coredump_enable = 0;
        } else {
            ret = property_get(aee_property.key, aee_property.value, NULL);
            if (0 != ret) {
                ALOGI("key:(%s)-value:(%s)\n", aee_property.key, aee_property.value);
                if (0 == strcmp(aee_property.value, "mtk"))
                    coredump_enable = 1;
                else
                    coredump_enable = 2;
            } else {
                ALOGI("get coredump_mode property(%s) failed\n", coredump_mode.key);
                coredump_enable = 2;
            }
        }
    } else {
        ALOGE("get system build type(%s) failed\n", build_type.key);
        coredump_enable = 0;
    }
#endif
    ALOGI("Connectivity coredump mode: %d\n", coredump_enable);
    sprintf(coredump_mode.value, "%d", coredump_enable);

    ret = property_set(coredump_mode.key, coredump_mode.value);
    if (0 != ret)
        ALOGE("set property(%s) to %s failed,ret:%d, errno:%d\n",
            coredump_mode.key, coredump_mode.value, ret, errno);
    else
        ALOGI("set property(%s) to %s succeed.\n",
            coredump_mode.key, coredump_mode.value);

    /*set coredump mode to kernel driver*/
    ioctl(g_wmt_fd, WMT_IOCTL_WMT_COREDUMP_CTRL, coredump_enable);

    return 0;
}


static int launcher_get_wmt_cfg_infor(void)
{
    FILE * file = NULL;
    int ret = -1;
    char *str = NULL;
    char line[NAME_MAX];

    file = fopen(CUST_COMBO_CFG_FILE, "r");
    if (NULL == file) {
        ALOGE("%s cannot be opened, errno:%d\n", CUST_COMBO_CFG_FILE, errno);
        return -2;
    }
    ret = 0;
    do {
        str = fgets(line, NAME_MAX, file);
        if (NULL == str) {
            ALOGI("NULL is returned, eighter EOF or error maybe found\n");
            break;
        }
        launcher_parser_wmt_cfg(line);
        memset(line, 0, NAME_MAX);

    } while (str != NULL);

    if (NULL != file) {
        if (0 == fclose(file))
            ALOGI("close %s succeed\n", CUST_COMBO_CFG_FILE);
        else
            ALOGI("close %s failed, errno:%d\n", CUST_COMBO_CFG_FILE, errno);
    }
    return ret;
}


static int launcher_get_chip_info_index(int chip_id)
{
    int index = -1;
    int left = 0;
    int middle = 0;
    int right = sizeof (g_chip_mode_info) / sizeof (g_chip_mode_info[0]) - 1;

    if ((chip_id < g_chip_mode_info[left].chip_id) || (chip_id > g_chip_mode_info[right].chip_id))
        return index;

    middle = (left + right) / 2;

    while (left <= right) {
        if (chip_id > g_chip_mode_info[middle].chip_id)
            left = middle + 1;
        else if (chip_id < g_chip_mode_info[middle].chip_id)
            right = middle - 1;
        else {
            index = middle;
            break;
        }
        middle = (left + right) / 2;
    }

    if (0 > index)
        ALOGE("no supported chip_id found\n");
    else
        ALOGI("index:%d, chip_id:0x%x\n", index, g_chip_mode_info[index].chip_id);

    return index;
}

static void launcher_combine_cfg_name(int chip_id, char *cfg_file_path)
{
    int index = -1;
    int stp_mode = g_stp_param_config.stp_mode;

    index = launcher_get_chip_info_index(chip_id);
    if ((stp_mode <= STP_MIN) || (STP_SDIO < stp_mode)) {
        ALOGI("STP Mode is not set, fetching default mode...\n");
        if (0 <= index)
            g_stp_param_config.stp_mode = g_chip_mode_info[index].stp_mode;
        else
            g_stp_param_config.stp_mode = -1;
    }

    if ((0 <= index) && (NULL != cfg_file_path)) {
        memset(g_wmt_cfg_name, 0, sizeof(g_wmt_cfg_name));
        strncpy(g_wmt_cfg_name, g_chip_mode_info[index].ant_mode.cfg_item_value, sizeof(g_wmt_cfg_name) - 1);
        g_wmt_cfg_name[strlen(g_chip_mode_info[index].ant_mode.cfg_item_value)] = '\0';
    } else
        memset(g_wmt_cfg_name, 0, sizeof(g_wmt_cfg_name));

    ALOGI("chip_id(0x%04x), default Mode(%d), strlen(g_wmt_cfg_name)(%zd), g_wmt_cfg_name(%s)\n",
           chip_id, g_stp_param_config.stp_mode, strlen(g_wmt_cfg_name), g_wmt_cfg_name);

}

static void launcher_set_wmt_cfg_infor(int chip_id, char *cfg_file_path) {
    if (!launcher_get_wmt_cfg_infor()) {
        launcher_combine_cfg_name(chip_id, cfg_file_path);
        /* send WMT config name configuration to driver */
        ioctl(g_wmt_fd, WMT_IOCTL_WMT_CFG_NAME, g_wmt_cfg_name);
    } else
         ALOGE("launcher_get_wmt_cfg_infor fail\n");
}

static void launcher_set_uart_port_name(char *stp_dev) {
    char *uart_name = NULL;

    /* send uart name to driver*/
    if (stp_dev) {
        uart_name = strstr(stp_dev, "tty");
        if (!uart_name)
            ALOGE("no uart name found in %s\n", stp_dev);
        else
            ALOGI("uart name %s\n", uart_name);
    }

    if (!uart_name) {
        uart_name = "ttyMT2";
        ALOGI("use default uart %s\n", uart_name);
    }

    ioctl(g_wmt_fd, WMT_IOCTL_PORT_NAME, uart_name);
}

static void* launcher_pwr_on_thread(void * arg)
{
    int retry_counter = 0;
    int ret = -1;
    int chip_id = *(int*)arg;

    pthread_setname_np(pthread_self(), "pwr_on_conn");

    ALOGI("enter power on connsys flow");
    do {
        ret = ioctl(g_wmt_fd, WMT_IOCTL_LPBK_POWER_CTRL, 1);
        if (0 == ret)
            break;
        ioctl(g_wmt_fd, WMT_IOCTL_LPBK_POWER_CTRL, 0);
        ALOGI("power on %x failed, retrying, retry counter:%d\n", chip_id, retry_counter);
        usleep(1000000);
        retry_counter++;
    } while (retry_counter < 20);

    pthread_detach(g_thread_handle);
    g_thread_handle = -1;

    return NULL;
}

static void launcher_pwr_on_chip(int *chip_id) {
    if (pthread_create(&g_thread_handle, NULL, launcher_pwr_on_thread, chip_id)) {
        ALOGE("create pwr on thread fail\n");
    } else
        ALOGI("create pwr on thread ok\n");
}
static void* launcher_set_fwdbg_thread(void * arg) {
    int i_ret = -1;
    int flag = *(int*)arg;

    pthread_setname_np(pthread_self(), "dump_fwemi_log");
    ALOGI("dump firmware dbg log from emi buffer ");
    i_ret = ioctl(g_wmt_fd, WMT_IOCTL_FW_DBGLOG_CTRL, flag);
    if (i_ret < 0) {
        ALOGI("ioctl error: err msg: %s\n", strerror(errno));
        pthread_detach(g_thread_handle);
        g_thread_handle = -1;
    }
    return NULL;
}

static void sig_hup(int sig) {
    fprintf(stderr, "sig_hup...(%d)\n", sig);
}

static void sig_term(int sig)
{
    fprintf(stderr, "sig_term...(%d)\n", sig);
    __io_canceled = 1;
    ioctl(g_wmt_fd, WMT_IOCTL_SET_LAUNCHER_KILL, 1);
}

static void launcher_set_signal_handler(void) {
    struct sigaction sa;

    /*set signal handler*/
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags   = SA_NOCLDSTOP;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    sa.sa_handler = sig_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    sa.sa_handler = sig_hup;
    sigaction(SIGHUP, &sa, NULL);
}

static void launcher_check_driver_ready(void) {
    int ret = -1;
    struct sys_property sp;

    sp.key = WCN_DRIVER_READY_PROP;
    sp.def_value = NULL;

    do {
        ret = property_get(sp.key, sp.value, sp.def_value);
        if (0 >= ret)
            ALOGE("get property(%s) failed i_ret:%d\n", sp.key, ret);
        else {
            ALOGI("get property(%s) is %s\n", sp.key, sp.value);
            if (!strcmp(sp.value, "yes"))
                break;
        }
        usleep(300000);
    } while (1);
}

static void launcher_open_wmt_device(void) {
    do {
        g_wmt_fd = open(CUST_COMBO_WMT_DEV, O_RDWR | O_NOCTTY);
        if (g_wmt_fd < 0) {
            ALOGE("Can't open device node(%s) error:%d \n", CUST_COMBO_WMT_DEV, g_wmt_fd);
            usleep(300000);
        } else
            break;
    }while(1);

    ALOGI("open device node succeed.(Node:%s, fd:%d) \n", CUST_COMBO_WMT_DEV, g_wmt_fd);
}

static int launcher_query_chip_id(void) {
    int chip_id = -1;
    int ret = -1;
    struct sys_property chipid_prop;

    chipid_prop.key = WCN_COMBO_CHIP_ID_PROP;
    chipid_prop.def_value = NULL;

    do {
        /*read from system property*/
        ret = property_get(chipid_prop.key, chipid_prop.value, chipid_prop.def_value);
        if (0 != ret) {
            chip_id = strtoul(chipid_prop.value, NULL, 16);
            ALOGI("key:(%s)-value:(%s),chip_id:0x%04x\n",
                   chipid_prop.key, chipid_prop.value, chip_id);
        } else {
            ALOGE("get chip_id property(%s) failed\n", chipid_prop.key);
            /* we do not return here, use another way to get chip id*/
        }

        if (-1 != chip_id)
            break;

        chip_id = ioctl(g_wmt_fd, WMT_IOCTL_WMT_QUERY_CHIPID, NULL);
        ALOGI("chip_id from kernel by ioctl:0x%04x\n", chip_id);
        if (-1 != chip_id)
            break;
        usleep(300000);
    }while(1);

    ALOGI("chip_id:0x%04x\n", chip_id);

    return chip_id;
}

static enum wmt_chip_type launcher_check_chip_type(int chip_id)
{
    enum wmt_chip_type chip_type = WMT_CHIP_TYPE_INVALID;

    if (chip_id <= 0) {
        ALOGE("invalid chip id(%d)\n", chip_id);
        goto done;
    }
    if ((0x6620 == chip_id) || (0x6628 == chip_id) ||
        (0x6630 == chip_id) || (0x6632 == chip_id))
        chip_type = WMT_CHIP_TYPE_COMBO;
    else
        chip_type = WMT_CHIP_TYPE_SOC;
done:
    return chip_type;
}
static void launcher_init_stp_parameter(struct stp_params_config *stp_param_config, enum wmt_chip_type chip_type) {
    stp_param_config->fm_mode = 2;

    if (WMT_CHIP_TYPE_SOC == chip_type)
        stp_param_config->stp_mode = STP_BTIF_FULL;
    if (WMT_CHIP_TYPE_COMBO == chip_type) {
        stp_param_config->stp_mode = STP_SDIO;
        stp_param_config->uart_config.baud_rate = 4000000;
        stp_param_config->uart_config.fc = UART_DISABLE_FC;
        stp_param_config->uart_config.parity = 0;
        stp_param_config->uart_config.stop_bit = 0;
        memset(stp_param_config->stp_dev, '\0', sizeof(stp_param_config->stp_dev));
    }
#if (!CUST_MULTI_PATCH && !CUST_PATCH_SEARCH)
    memset(stp_param_config->patch_name, '\0', sizeof(stp_param_config->patch_name));
#endif
    memset(stp_param_config->patch_path, '\0', sizeof(stp_param_config->patch_path));
}

static void launcher_print_stp_parameter(struct stp_params_config *stp_param_config) {
    ALOGI("fm mode:%d\n", stp_param_config->fm_mode);
    ALOGI("stp mode:%d\n", stp_param_config->stp_mode);
    ALOGI("patch path:%s\n", stp_param_config->patch_path);
#if (!CUST_MULTI_PATCH && !CUST_PATCH_SEARCH)
    ALOGI("patch name:%d\n", stp_param_config->patch_name);
#endif
    if ((stp_param_config->stp_mode == STP_UART_FULL) ||
        (stp_param_config->stp_mode == STP_UART_MAND)) {
        ALOGI("stp uart dev:%s\n", stp_param_config->stp_dev);
        ALOGI("stp uart baud rate:%d\n", stp_param_config->uart_config.baud_rate);
        ALOGI("stp uart fc:%d\n", stp_param_config->uart_config.fc);
        ALOGI("stp uart parity:%d\n", stp_param_config->uart_config.parity);
        ALOGI("stp uart stop bit:%d\n", stp_param_config->uart_config.stop_bit);
    }
}
/*
 * -m: mode (SDIO/UART)
 * -d: uart device node
 * -b: baudrate
 * -c: enable SW FC or not
 * -p: patch folder path
 * -n: patch file name (fullpath) 
 *
 */
static void launcher_set_parameter_to_driver(int argc, char *argv[], int chip_id)
{
    int opt;
    int i = 0;
    static const char *op_string = "m:d:b:c:p:n:?";
    enum wmt_chip_type chip_type;

    chip_type = launcher_check_chip_type(chip_id);
    if (chip_type == WMT_CHIP_TYPE_INVALID) {
        ALOGE("unknow chip type, may be chid id is invalid\n");
        chip_type = WMT_CHIP_TYPE_SOC;
    }
    launcher_init_stp_parameter(&g_stp_param_config, chip_type);

    opt = getopt(argc, argv, op_string);
    while (opt != -1) {
        switch (opt) {
            case 'm':
                g_stp_param_config.stp_mode = atoi(optarg);
            break;
            case 'd':
                strcpy(g_stp_param_config.stp_dev, optarg);
            break;
            case 'b':
                g_stp_param_config.uart_config.baud_rate = atoi(optarg);
            break;
            case 'c':
                g_stp_param_config.uart_config.fc = atoi(optarg);
            break;
            case 'p':
                strcpy(g_stp_param_config.patch_path, optarg);
                ALOGI("patch path:%s\n", g_stp_param_config.patch_path);
            break;
#if (!CUST_MULTI_PATCH && !CUST_PATCH_SEARCH)
            case 'n':
                strcpy(g_stp_param_config.patch_name, optarg);
            break;
#endif
            case '?':
            default:
                launcher_display_usage();
            break;
        }
        opt = getopt(argc, argv, op_string);
    }

#if LAUNCHER_DEBUG
    ALOGI("argc = %d, optind= %d\n", argc, optind);
    {
        int i = 0;
        for (i = 0; i < argc; i++)
            ALOGI("arg[%d] = %s\n", i, argv[i]);
    }
#endif

    if (0 != launcher_stp_param_valid_check(&g_stp_param_config, chip_type)) {
        ALOGE("stp param is invalid, use default:\n");
        launcher_print_stp_parameter(&g_stp_param_config);
        ALOGE("if this default param is not correct,please see usage help:\n");
        launcher_display_usage();
    }
    if (WMT_CHIP_TYPE_COMBO == chip_type) {
        ioctl(g_wmt_fd, WMT_IOCTL_WMT_TELL_CHIPID, chip_id);
        ALOGI("set chip_id(0x%x) to driver\n", chip_id);
        launcher_set_wmt_cfg_infor(chip_id, &g_stp_param_config.patch_path[0]);
        if ((g_stp_param_config.stp_mode == STP_UART_FULL) ||
            (g_stp_param_config.stp_mode == STP_UART_MAND))
            launcher_set_uart_port_name(&g_stp_param_config.stp_dev[0]);
    }
#if (!CUST_MULTI_PATCH && !CUST_PATCH_SEARCH)
    /* send default patch file name path to driver */
    ioctl(g_wmt_fd, WMT_IOCTL_SET_PATCH_NAME, g_stp_param_config.patch_name);
#endif
    /* send hardware interface configuration to driver */
    ioctl(g_wmt_fd, WMT_IOCTL_SET_STP_MODE,
          ((g_stp_param_config.uart_config.baud_rate & 0xFFFFFF) << 8) |
          ((g_stp_param_config.fm_mode & 0x0F) << 4) |
          (g_stp_param_config.stp_mode & 0x0F));

    launcher_set_coredump_flag();
    ioctl(g_wmt_fd, WMT_IOCTL_SET_LAUNCHER_KILL, 0);

}

static int launcher_get_fw_debug_orign_infor(int chip_id) {
    int ret = -1;
    enum wmt_chip_type chip_type;

    chip_type = launcher_check_chip_type(chip_id);

    if (chip_type == WMT_CHIP_TYPE_SOC) {
        g_fw_debug_infor.log_retry = 0;
        g_fw_debug_infor.dump_retry = 0;
        g_fw_debug_infor.fw_log_ctrl = 0;
        g_fw_debug_infor.fw_dynamic_dump = 0;
        g_fw_debug_infor.bitmap = 0;
        memset(g_fw_debug_infor.fw_log_state_orig, '\0', sizeof(g_fw_debug_infor.fw_log_state_orig));
        memset(g_fw_debug_infor.fw_log_state_new, '\0', sizeof(g_fw_debug_infor.fw_log_state_new));
        memset(g_fw_debug_infor.fw_dump_state_orig, '\0', sizeof(g_fw_debug_infor.fw_dump_state_orig));
        memset(g_fw_debug_infor.fw_dump_state_new, '\0', sizeof(g_fw_debug_infor.fw_dump_state_new));

        ret = property_get(WCN_FW_DBG_LOG_PROP, g_fw_debug_infor.fw_log_state_orig, NULL);
        if (0 >= ret)
            ALOGE("get property(%s) failed ret:%d\n", WCN_FW_DBG_LOG_PROP, ret);
        else
            ALOGI("get property (%s) is %s\n", WCN_FW_DBG_LOG_PROP, g_fw_debug_infor.fw_log_state_orig);

        ret = property_get(WCN_DYNAMIC_DUMP_PROP, g_fw_debug_infor.fw_dump_state_orig, NULL);
        if (0 >= ret)
            ALOGE("get property(%s) failed ret:%d\n", WCN_DYNAMIC_DUMP_PROP, ret);
        else
            ALOGI("get property (%s) is %s\n", WCN_DYNAMIC_DUMP_PROP, g_fw_debug_infor.fw_dump_state_orig);
    }
    return 0;
}

static int launcher_set_fw_log_ctrl(void) {
    int ret = -1;
    int fw_debug_enable = 0;

    if (g_fw_debug_infor.bitmap & 0x1) {
        ALOGI("fw log ctrl flag has been set\n");
        return 0;
    }

    if ((g_fw_debug_infor.fw_log_ctrl == 1) ||
         (g_fw_debug_infor.log_retry > WCN_FW_DEBUG_CLRL_RETRY_LIMIT)) {
        ALOGI("retry: %d\n", g_fw_debug_infor.log_retry);

        if (!strcmp(g_fw_debug_infor.fw_log_state_new, "yes")) {
            fw_debug_enable = 1;
            if (pthread_create(&g_thread_handle, NULL, launcher_set_fwdbg_thread, &fw_debug_enable))
                ALOGE("create enable firmware dbglog thread fail\n");
        } else {
            ret = ioctl(g_wmt_fd, WMT_IOCTL_FW_DBGLOG_CTRL, fw_debug_enable);
            /* if do this jude,it will confuse user beacuse kernel return -1 */
            /* if (ret < 0)*/
            /*   ALOGE("ioctl error: err msg: %s\n", strerror(errno));*/
        }
        g_fw_debug_infor.bitmap |= (0x1 << 0);
    } else if (g_fw_debug_infor.log_retry++ <= WCN_FW_DEBUG_CLRL_RETRY_LIMIT) {
        ret = property_get(WCN_FW_DBG_LOG_PROP, g_fw_debug_infor.fw_log_state_new, NULL);
        if (0 >= ret)
            ALOGE("get property(%s) failed ret:%d\n", WCN_FW_DBG_LOG_PROP, ret);
        else {
            ALOGI("get property(%s) is %s\n", WCN_FW_DBG_LOG_PROP, g_fw_debug_infor.fw_log_state_new);
            if (strcmp(g_fw_debug_infor.fw_log_state_new, g_fw_debug_infor.fw_log_state_orig))
                g_fw_debug_infor.fw_log_ctrl = 1;
        }
    } else
        ALOGV("retry finish\n");
    return 0;
}

static int launcher_set_fw_dynamic_dump(void) {
    int ret = -1;

    if (g_fw_debug_infor.bitmap & (0x1 << 1)) {
        ALOGI("fw dynamic ctrl flag has been set\n");
        return 0;
    }

    if ((g_fw_debug_infor.fw_dynamic_dump == 1) ||
         (g_fw_debug_infor.dump_retry > WCN_FW_DEBUG_CLRL_RETRY_LIMIT)) {
        ALOGI("dump_retry: %d\n", g_fw_debug_infor.dump_retry);

        if (g_fw_debug_infor.fw_dynamic_dump == 1) {
        /* it is only meaningfull to do ioctl in this case*/
            ret = ioctl(g_wmt_fd, WMT_IOCTL_DYNAMIC_DUMP_CTRL, g_fw_debug_infor.fw_dump_state_new);
            ALOGI("%d, %zd,\n", PROPERTY_VALUE_MAX, strlen(g_fw_debug_infor.fw_dump_state_new));
            if (ret < 0)
                ALOGE("ioctl error: err msg: %s\n", strerror(errno));
        }
        g_fw_debug_infor.bitmap |= (0x1 << 1);
    } else if (g_fw_debug_infor.dump_retry++ <= WCN_FW_DEBUG_CLRL_RETRY_LIMIT) {
        ret = property_get(WCN_DYNAMIC_DUMP_PROP, g_fw_debug_infor.fw_dump_state_new, NULL);
        if (0 >= ret)
            ALOGE("get property(%s) failed ret:%d\n", WCN_DYNAMIC_DUMP_PROP, ret);
        else {
            ALOGI("get property(%s) is %s\n", WCN_DYNAMIC_DUMP_PROP, g_fw_debug_infor.fw_dump_state_new);
            if (strcmp(g_fw_debug_infor.fw_dump_state_new, g_fw_debug_infor.fw_dump_state_orig))
                g_fw_debug_infor.fw_dynamic_dump = 1;
        }
    } else
        ALOGV("retry finish\n");

    return 0;
}

static void launcher_set_fw_debug_infor(int chip_id) {
    enum wmt_chip_type chip_type = WMT_CHIP_TYPE_INVALID;

    chip_type = launcher_check_chip_type(chip_id);
    if (chip_type == WMT_CHIP_TYPE_SOC) {
        launcher_set_fw_log_ctrl();
        launcher_set_fw_dynamic_dump();
    }
}

int main(int argc, char *argv[]) {
    int ld, err;
    struct pollfd fds[2];
    int fd_num = 0;
    int len = 0;
    int chip_id = -1;

    launcher_check_driver_ready();
    launcher_open_wmt_device();
    chip_id = launcher_query_chip_id();

    launcher_set_parameter_to_driver(argc, argv, chip_id);

    launcher_pwr_on_chip(&chip_id);

    launcher_set_signal_handler();

    launcher_get_fw_debug_orign_infor(chip_id);

    fds[0].fd = g_wmt_fd; /* stp_wmt fd */
    fds[0].events = POLLIN | POLLRDNORM; /* wait read events */
    ++fd_num;

    while (!__io_canceled) {
        launcher_set_fw_debug_infor(chip_id);
        fds[0].revents = 0;
        err = poll(fds, fd_num, 2000);  /* 2 seconds */
        if (err < 0) {
            if (errno == EINTR)
                continue;
            else {
                ALOGE("poll error:%d errno:%d, %s\n", err, errno, strerror(errno));
                break;
            }
        } else if (!err)
            continue;


        if (fds[0].revents & POLLIN) {
            if (g_wmt_fd < 0)
                break;
            memset(g_cmd_str, 0, sizeof(g_cmd_str));
            len = read(g_wmt_fd, g_cmd_str, sizeof(g_cmd_str)-1);
            if (len <= 0 || len >= (int)sizeof(g_cmd_str)) {
                ALOGE("POLLIN(%d) but read fail:%d\n", g_wmt_fd, len);
                continue;
            }
            g_cmd_str[len] = '\0';
            /*ALOGI("rx_cmd_str:%s\n", g_cmd_str);*/
            err = launcher_handle_cmd(&g_stp_param_config, g_cmd_str, len);
            if (!err) {
                /* ALOGI("handle_cmd(%s), respond ok \n", g_cmd_str); */
                snprintf(g_resp_str, sizeof(g_resp_str), "ok");
            } else {
                if (err == 1)
                    snprintf(g_resp_str, sizeof(g_resp_str), "cmd not found");
                else
                    snprintf(g_resp_str, sizeof(g_resp_str), "resp_%d", err);
            }
            ALOGI("cmd(%s) resp(%s)\n", g_cmd_str, g_resp_str);
            len = write(g_wmt_fd, g_resp_str, strlen(g_resp_str));
            if (len != (int)strlen(g_resp_str))
                fprintf(stderr, "write resp(%d) fail: len(%d), errno(%d, %s)\n",
                        g_wmt_fd, len, errno, (len == -1) ? strerror(errno) : "");
        }
    }

clean_up:

    if (g_wmt_fd >= 0) {
        close(g_wmt_fd);
        g_wmt_fd = -1;
    }

    if (((g_stp_param_config.stp_mode == STP_UART_MAND) ||
        (g_stp_param_config.stp_mode == STP_UART_FULL)) && g_tty_fd >= 0) {
        /* Restore TTY line discipline */
        ld = N_TTY;
        if (ioctl(g_tty_fd, TIOCSETD, &ld) < 0) {
            ALOGE("Can't restore line discipline");
            exit(1);
        }

        close(g_tty_fd);
        g_tty_fd = -1;
    }

    if (g_thread_handle != -1) {
        pthread_detach(g_thread_handle);
        g_thread_handle = -1;
    }

    return 0;
}

