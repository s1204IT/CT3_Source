package com.mediatek.bluetooth.dtt;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.*;
import android.util.Log;

public class SettingsFragment extends PreferenceFragment {

    // preference
    protected ListPreference mPrefTestSituation;
    protected Preference mPrefBtScCustPath;
    protected Preference mPrefSysToolOpenMtkLogger;
    protected Preference mPrefSysToolOpenEngMode;

    // parent activity
    private MainActivity mParentActivity;

    @Override
    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        // config parent activity
        this.mParentActivity = (MainActivity)this.getActivity();

        // 使用自動的 shared preference name
        PreferenceManager pm = this.getPreferenceManager();
        pm.setSharedPreferencesName(Const.PREF_NAME);
        pm.setSharedPreferencesMode(Context.MODE_PRIVATE);
        // Load the preferences from an XML resource
        this.addPreferencesFromResource(R.xml.preferences);

        // 設定 ui component
        this.mPrefTestSituation = (ListPreference)this.findPreference(DataModel.F_TEST_SITUATION);
        this.mPrefBtScCustPath = this.findPreference(DataModel.F_BTSC_CUST_PATH);
        this.mPrefSysToolOpenMtkLogger = this.findPreference(DataModel.F_SYS_TOOL_OPEN_MTKLOGGER);
        this.mPrefSysToolOpenEngMode = this.findPreference(DataModel.F_SYS_TOOL_OPEN_ENGMODE);
        this.updateUI(null);
    }

    public void registerOnSharedPreferenceChangeListener(SharedPreferences.OnSharedPreferenceChangeListener listener){
        this.getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(listener);
    }

    /**
     * called by parent activity (MainActivity)
     * @param dataModel
     */
    protected void updateUI(DataModel dataModel){

        Log.i(Const.TAG, "SettingsFragment.updateUI()[+]");

        if (this.getActivity() == null || !this.isAdded()){
            Log.w(Const.TAG, "SettingsFragment.updateUI(): wrong state, isAdded[" + this.isAdded() + "], activity:" + this.getActivity());
            return;
        }

        // update preference summary
        int testSituation;
        String btScCustPath = "";
        if (dataModel == null) {
            String pref = this.getPreferenceManager().getSharedPreferences()
                    .getString(DataModel.F_TEST_SITUATION, Integer.toString(Const.TS_DEFAULT));
            testSituation = Integer.parseInt(pref);
        }
        else {
            testSituation = dataModel.mTestSituation;
            if (dataModel.mBtScCustPath != null) {
                btScCustPath = this.getString(R.string.act_set_pref_btsc_cust_path_summary, dataModel.mBtScCustPath);
            }
        }

        // update preference summary
        String[] labels = this.getResources().getStringArray(R.array.act_set_pref_test_situation_entries_labels);
        String[] summaries = this.getResources().getStringArray(R.array.act_set_pref_test_situation_entries_summaries);
        this.mPrefTestSituation.setTitle(this.getString(R.string.act_set_pref_test_situation_title, labels[testSituation]));
        this.mPrefTestSituation.setSummary(summaries[testSituation]);

        // update cust config path
        boolean isCust = (testSituation == Const.TS_CUST);
        this.mPrefBtScCustPath.setEnabled(isCust);
        this.mPrefBtScCustPath.setSummary(btScCustPath);
    }

    /**
     * 1. 處理 Click 事件 ( 在 onSharedPreferenceChanged 之後執行 ) <br>
     * 2. 針對 persistent=false 者, 只能在此處理
     */
    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference)
    {
        Log.i(Const.TAG, "onPreferenceTreeClick: " + preference.getKey());

        String key = preference.getKey();
        if (DataModel.F_BTSC_CUST_PATH.equals(key))
        {
            this.mParentActivity.onSelectBtScCustPath();
            return true;
        }
        if (DataModel.F_SYS_TOOL_OPEN_MTKLOGGER.equals(key)){
            Util.openMtkLogger(this.getActivity());
            return true;
        }
        if (DataModel.F_SYS_TOOL_OPEN_ENGMODE.equals(key)){
            Util.openEngineerMode(this.getActivity());
            return true;
        }
        return false;
    }
}
