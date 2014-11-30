package com.kii.yankon;

import com.kii.cloud.storage.Kii;

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
        Kii.initialize("c99e04f1", "3ebdc0472c0c705bc50eaf1756061b8b", Kii.Site.CN);
    }
}
