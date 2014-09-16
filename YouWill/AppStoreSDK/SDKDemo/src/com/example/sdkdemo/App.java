package com.example.sdkdemo;

import android.app.Application;
import android.content.pm.PackageManager.NameNotFoundException;

import com.kii.appstore.secure.AppStoreSecureSDK;

public class App extends Application {

    @Override
    public void onCreate() {
        super.onCreate();
        String key = null;
        try {
            key = AppStoreSecureSDK.getSecureKey(getApplicationContext());
        } catch (NameNotFoundException e) {
            //The app store client or sandbox testing client is not installed
            key = null;
        }
        if (!"test".equals(key)) {
            System.exit(0);
        }
    }
    
}
