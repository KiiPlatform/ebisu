package com.example.sdkdemo;

import android.app.Application;

import com.kii.appstore.secure.AppStoreSecureSDK;

public class App extends Application {

    @Override
    public void onCreate() {
        super.onCreate();
        String key = AppStoreSecureSDK.getSecureKey(getApplicationContext());
        if (!"test".equals(key)) {
            System.exit(0);
        }
    }
    
}
