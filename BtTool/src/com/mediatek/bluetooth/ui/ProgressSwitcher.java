package com.mediatek.bluetooth.ui;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.ViewSwitcher;
import com.mediatek.bluetooth.dtt.R;

/**
 * Created by MTK01635 on 2015/4/23.
 */
public class ProgressSwitcher {

    private static final int VID_LOADING = 0;
    private static final int VID_TARGET = 1;

    private Activity mActivity;
    private ViewSwitcher mSwitcher; // 負責 Target 與 Loading 的切換

    public static ProgressSwitcher wrap(Activity activity, View targetView){

        return new ProgressSwitcher(activity, targetView);
    }
    public static ProgressSwitcher wrap(Activity activity, int viewId){

        return new ProgressSwitcher(activity, activity.findViewById(viewId));
    }

    /**
     * Constructor
     *
     * @param activity
     * @param targetView
     */
    private ProgressSwitcher(Activity activity, View targetView){

        // 紀錄 Context (Activity)
        this.mActivity = activity;
        this.mSwitcher = new ViewSwitcher(this.mActivity);

        // 建立 Loading Layout
        View loadingLayout = this.mActivity.getLayoutInflater().inflate(R.layout.progress_switcher, null, false);
        //loadingLayout.setVisibility(View.GONE);

        // 需先移除 TargetView 的 Parent 否則會: "The specified child already has a parent. You must call removeView() on the child's parent first."
        // 將 ViewSwitcher 替換原本 TargetView 的位置
        ViewParent parent = targetView.getParent();
        if (parent != null && parent instanceof ViewGroup) {

            ViewGroup group = (ViewGroup)parent;
            int index = group.indexOfChild(targetView);
            group.removeView(targetView);
            group.addView(this.mSwitcher, index);
        }
        //else {
        //    this.mActivity.setContentView(this.mSwitcher);
        //}

        // 設定 ViewSwitcher
        ViewSwitcher.LayoutParams params = new ViewSwitcher.LayoutParams(ViewSwitcher.LayoutParams.MATCH_PARENT, ViewSwitcher.LayoutParams.MATCH_PARENT);
        this.mSwitcher.setLayoutParams(params);
        this.mSwitcher.addView(loadingLayout, VID_LOADING);
        this.mSwitcher.addView(targetView, VID_TARGET);
        this.mSwitcher.setDisplayedChild(VID_TARGET);
    }

    public View getReplaceView(){

        return this.mSwitcher;
    }

    public void showLoading(){

        this.mSwitcher.setDisplayedChild(VID_LOADING);
    }

    public void hideLoading(){

        this.mSwitcher.setDisplayedChild(VID_TARGET);
    }
}
