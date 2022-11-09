package com.mediatek.bluetooth.dtt;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Created by MTK01635 on 2015/4/23.
 */
public class DataModel {

// 需要存檔 - SharedPrefernces
    public static final String F_TEST_SITUATION = "pref_test_situation";
    public static final String F_BTSC_CUST_PATH = "pref_btsc_cust_path";
    public static final String F_SYS_TOOL_OPEN_MTKLOGGER = "pref_systool_open_mtklogger";
    public static final String F_SYS_TOOL_OPEN_ENGMODE = "pref_systool_open_engmode";

    public int mTestSituation = Const.TS_SQC;
    public String mBtScCustPath = Const.DEFAULT_CONFIG_DIR + Const.DEFAULT_STACK_CONF; // 當 Test Situation 為 Cust 時對應的 path

// 不需存檔
    public int mState = Const.STATE_INIT;

    /**
     * load preferences from storage
     *
     * @param context
     */
    public void load(Context context){

        // from SharedPreferences
        SharedPreferences preferences = Util.getSettingPreference(context);
        this.mTestSituation = Integer.parseInt(preferences.getString(F_TEST_SITUATION, Integer.toString(this.mTestSituation)));
        this.mBtScCustPath = preferences.getString(F_BTSC_CUST_PATH, this.mBtScCustPath);
    }

    /**
     * save preferences into storage
     * @param context
     */
    public void save(Context context){

        SharedPreferences preferences = Util.getSettingPreference(context);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putString(F_TEST_SITUATION, Integer.toString(this.mTestSituation));
        editor.putString(F_BTSC_CUST_PATH, this.mBtScCustPath);
        editor.commit();
    }
}
