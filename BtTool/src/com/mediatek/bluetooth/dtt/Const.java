package com.mediatek.bluetooth.dtt;

/**
 * Created by MTK01635 on 2015/4/23.
 */
public class Const {

    // log tag
    public static final String TAG = "BT.DTT";

    // global preference name
    protected static final String PREF_NAME = "btdtt.conf";

    public static final int STATE_INIT = 0;
    public static final int STATE_READY = 1;
    public static final int STATE_BUSY = 2;

    // Test Situation
    public static final int TS_DEFAULT = 0;
    public static final int TS_SQC = 1;
    public static final int TS_DEBUG = 2;
    public static final int TS_CUST = 3;

    // default file & log file and directory
    public static final String SHARE_FILE_DIR = "/sdcard/";
    public static final String BT_CONF_FILE = "btsc";
    public static final String DEFAULT_CONFIG_DIR = "/etc/bluetooth/";
    public static final String DEFAULT_STACK_CONF = "bt_stack.conf";
    //public static final String DEFAULT_FW_CONF = "bt_fw.conf";
}
