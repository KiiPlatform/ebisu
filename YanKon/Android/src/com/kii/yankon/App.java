package com.kii.yankon;

import android.app.Application;

/**
 * Created by tian on 14/11/20:上午11:30.
 */
public class App extends Application {
    private static App mApp;
    public static App getApp() {
        return mApp;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mApp = this;
    }
}
