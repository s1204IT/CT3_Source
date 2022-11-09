package com.mediatek.bluetooth.dtt;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.*;
import android.util.Log;
import com.mediatek.bluetooth.ui.ProgressSwitcher;

public class MainActivity extends Activity implements Handler.Callback, SharedPreferences.OnSharedPreferenceChangeListener {

    // Handler Callback Event
    private static final int MSG_INIT = 1;
    private static final int MSG_UPDATE_UI = 2;
    private static final int MSG_UPDATE_TEST_SITUATION = 3;

    // Data Model
    private DataModel mModel = null;

    // UI Components
    private ProgressSwitcher mProgressSwitcher;
    private SettingsFragment mSettings;

    // Handler & Thread
    protected Handler mUiHandler;
    protected Handler mBgHandler;
    private HandlerThread mBgHandlerThread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {

        Log.i(Const.TAG, "MainActivity.onCreate()[+]");

        // init
        super.onCreate(savedInstanceState);
        this.setContentView(R.layout.activity_main);
        String title = this.getString(R.string.app_name) + " - v" + this.getString(R.string.app_version);
        this.setTitle(title);

        // data model
        this.mModel = new DataModel();

        // 設定 UI 元件: Settings
        this.mProgressSwitcher = ProgressSwitcher.wrap(this, R.id.am_container);
        this.mProgressSwitcher.showLoading();
        this.mSettings = (SettingsFragment)this.getFragmentManager().findFragmentById(R.id.act_main_settings);
        this.mSettings.registerOnSharedPreferenceChangeListener(this);

        // 設定 Handler
        this.mUiHandler = new Handler(this);
        this.mBgHandlerThread = new HandlerThread("btdtt.bg.handler", android.os.Process.THREAD_PRIORITY_FOREGROUND);
        this.mBgHandlerThread.start();
        this.mBgHandler = new Handler(this.mBgHandlerThread.getLooper(), this);

        // 檢驗環境
        this.mBgHandler.sendEmptyMessage(MSG_INIT);
        this.updateUI();
    }

    @Override
    protected void onDestroy() {

        this.mBgHandlerThread.quit();
        this.mBgHandler = null;
        this.mUiHandler = null;

        super.onDestroy();
    }

    /**
     * initialization
     */
    private void init(){
        // load DataModel
        //this.mModel.load(this);

        // 直接在第一次啟動就寫出設定
        this.updateTestSituation();
    }

    private void updateTestSituation(){

        // load new test situation from shared preferences
        this.mModel.load(this);

        // try to write test situation config file
        boolean ret = Util.writeTestSituation(this, this.mModel);
        // TODO error rollback
    }

    private void updateUI(){

        Log.i(Const.TAG, "MainActivity.updateUI()[+]");
        // 初始化中
        if (this.mModel.mState != Const.STATE_READY){
            this.mProgressSwitcher.showLoading();
        }
        else {
            this.mSettings.updateUI(this.mModel);
            this.mProgressSwitcher.hideLoading();
        }
    }

    @Override
    public boolean handleMessage(Message msg) {

        switch (msg.what){
            case MSG_INIT:
                this.init();
                this.mModel.mState = Const.STATE_READY;
                this.mUiHandler.sendEmptyMessage(MSG_UPDATE_UI);
                return true;
            case MSG_UPDATE_TEST_SITUATION:
                this.mModel.mState = Const.STATE_BUSY;
                this.mUiHandler.sendEmptyMessage(MSG_UPDATE_UI);
                this.updateTestSituation();
                this.mModel.mState = Const.STATE_READY;
                this.mUiHandler.sendEmptyMessage(MSG_UPDATE_UI);
                return true;
            /*
            case MSG_UPDATE_STORAGE_INFO:
                // update data model and repeat
                this.mModel.updateLogBytes();
                this.mBgHandler.sendEmptyMessageDelayed(MSG_UPDATE_STORAGE_INFO, 5000);
                this.mUiHandler.sendEmptyMessage(MSG_UPDATE_UI);
                return true;
            */
            case MSG_UPDATE_UI:
                this.updateUI();
                return true;
        }
        return false;
    }

    protected void onSelectBtScCustPath(){
        Util.openFileChooser(this, RC_CHOOSE_BTSC_FILE);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {

        Log.i(Const.TAG, "onSharedPreferenceChanged: " + key);
        // test situation changed
        if (DataModel.F_TEST_SITUATION.equals(key)){
            if (this.mBgHandler != null) {
                this.mBgHandler.sendEmptyMessage(MSG_UPDATE_TEST_SITUATION);
            }
        }
    }

    private static final int RC_CHOOSE_BTSC_FILE = 1;

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {

        if (requestCode == RC_CHOOSE_BTSC_FILE){

            if (resultCode != Activity.RESULT_OK) {
                // TODO ???
                Log.w(Const.TAG, "choose file failed");
                return;
            }

            Uri uri = data.getData();
            Log.i(Const.TAG, "cust.config:" + uri.getPath());
            if (requestCode == RC_CHOOSE_BTSC_FILE){
                this.mModel.mBtScCustPath = uri.getPath();
            }
            this.mModel.save(this);
            this.mBgHandler.sendEmptyMessage(MSG_UPDATE_TEST_SITUATION);
        }
    }
}
