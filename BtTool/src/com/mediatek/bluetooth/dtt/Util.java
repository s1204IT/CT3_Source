package com.mediatek.bluetooth.dtt;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Environment;
import android.os.StatFs;
import android.util.Log;
import android.widget.Toast;
import com.nononsenseapps.filepicker.FilePickerActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/**
 * Created by MTK01635 on 2015/4/23.
 *
 * TODO permission check ? 主要是 read, mtkbt 要 write ?
 *
 * http://stackoverflow.com/questions/5694933/find-an-external-sd-card-location
 * http://possiblemobile.com/2014/03/android-external-storage/
 *
 * http://stackoverflow.com/questions/14645189/android-get-list-of-all-available-storage
 * http://stackoverflow.com/questions/8133601/android-get-all-available-storage-devices
 */
public class Util {

    public static SharedPreferences getSettingPreference(Context context) {
        return context.getSharedPreferences(Const.PREF_NAME, Context.MODE_PRIVATE);
    }

    public static void openFileChooser(Activity activity, int requestCode){

        Intent i = new Intent(activity, FilePickerActivity.class);
        i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false);
        i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
        i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_FILE);
        i.putExtra(FilePickerActivity.EXTRA_START_PATH, Environment.getExternalStorageDirectory().getPath());
        activity.startActivityForResult(i, requestCode);
    }

    public static boolean writeTestSituation(Context context, DataModel dataModel){

        // TODO error handling
        switch(dataModel.mTestSituation){
            case Const.TS_DEFAULT:
                Util.copyRawResToFile(context, R.raw.btsc_0, Const.SHARE_FILE_DIR + Const.BT_CONF_FILE);
                return true;
            case Const.TS_SQC:
                Util.copyRawResToFile(context, R.raw.btsc_1, Const.SHARE_FILE_DIR + Const.BT_CONF_FILE);
                return true;
            case Const.TS_DEBUG:
                Util.copyRawResToFile(context, R.raw.btsc_2, Const.SHARE_FILE_DIR + Const.BT_CONF_FILE);
                return true;
            case Const.TS_CUST:
                StringBuilder content = new StringBuilder()
                        .append("OverrideConf=").append(dataModel.mBtScCustPath).append("\r\n");
                Util.writeStringToFile(content.toString(),  Const.SHARE_FILE_DIR + Const.BT_CONF_FILE);
                return true;
            default:
                Log.w(Const.TAG, "invalid test situation:" + dataModel.mTestSituation);
                return false;
        }
    }

    private static void writeStringToFile(String content, String filename){
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(filename, false);
            out.write(content.getBytes("utf8"));
            out.flush();
        }
        catch(Exception ex){
            Log.w(Const.TAG, "writeStringToFile() file:" + filename + " failed!", ex);
        }
        finally {
            try {
                if (out != null)    out.close();
            }
            catch(Exception ex){
                Log.w(Const.TAG, "close file:" + filename + " failed!", ex);
            }
        }
    }

    private static void copyRawResToFile(Context context, int rawResId, String filename){

        InputStream in = context.getResources().openRawResource(rawResId);
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(filename, false);
            byte[] buf = new byte[32];
            int read;
            while((read = in.read(buf)) > 0){
                out.write(buf, 0, read);
            }
        }
        catch(Exception ex){
            Log.w(Const.TAG, "copyToFile() file:" + filename + " failed!", ex);
        }
        finally {
            try {
                if (in != null)     in.close();
            }
            catch(Exception ex){
                Log.w(Const.TAG, "close raw res:" + rawResId + " failed!", ex);
            }
            try {
                if (out != null)    out.close();
            }
            catch(Exception ex){
                Log.w(Const.TAG, "close file:" + filename + " failed!", ex);
            }
        }
    }

//    public static void snedCmdToMtkLogger(Context context, boolean start){
//        /**
//         * Enable:  adb shell am broadcast -a com.mediatek.mtklogger.ADB_CMD -e cmd_name start --ei cmd_target 7
//         * Disable: adb shell am broadcast -a com.mediatek.mtklogger.ADB_CMD -e cmd_name stop --ei cmd_target 7
//         * MobileLog: 1 / ModemLog: 2 / NetworkLog: 4 / MET: 8 / GPS: 16
//         * 參考: com.mediatek.mtklogger.utils.Utils.java > LOG_TYPE_SET
//         */
//        try {
//            Log.i(Const.TAG, "sendCmdToMtkLogger:" + start);
//            Intent intent = new Intent("com.mediatek.mtklogger.ADB_CMD");
//            intent.putExtra("cmd_name", start ? "start" : "stop");
//            intent.putExtra("cmd_target", 7);
//            context.sendBroadcast(intent);
//        }
//        catch(Exception ex){
//            Log.e(Const.TAG, "sendCmdToMtkLogger() error: ", ex);
//            Toast.makeText(context, "Switch MtkLogger Err:" + ex.getMessage(), Toast.LENGTH_LONG).show();
//        }
//    }

    public static void openMtkLogger(Context context){
        try {
            //Intent intent = context.getPackageManager().getLaunchIntentForPackage("com.mediatek.mtklogger");
            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.setComponent(new ComponentName("com.mediatek.mtklogger", "com.mediatek.mtklogger.MainActivity"));
            context.startActivity(intent);
        }
        catch(Exception ex){
            Log.e(Const.TAG, "openMtkLogger() error: ", ex);
            Toast.makeText(context, "open MtkLogger Err:" + ex.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    public static void openEngineerMode(Context context){
        try {
            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.setComponent(new ComponentName("com.mediatek.engineermode", "com.mediatek.engineermode.EngineerMode"));
            context.startActivity(intent);
        }
        catch(Exception ex){
            Log.e(Const.TAG, "openEngineerMode() error: ", ex);
            Toast.makeText(context, "open EngMode Err:" + ex.getMessage(), Toast.LENGTH_LONG).show();
        }
    }
}
