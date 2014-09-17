package com.example.sdkdemo;

import android.app.Application;
import android.content.pm.PackageManager.NameNotFoundException;
import android.util.Log;

import com.kii.appstore.secure.AppStoreSecureSDK;

public class App extends Application {

    @Override
    public void onCreate() {
        super.onCreate();
        String key = null;
        AppStoreSecureSDK.setMode(AppStoreSecureSDK.MODE_SANDBOX);
        try {
            key = AppStoreSecureSDK.getSecureKey(getApplicationContext());
        } catch (NameNotFoundException e) {
            //The app store client or sandbox testing client is not installed
            Log.e("SDKDemo", "Cannot find appstore");
            key = null;
        }
        if (!"test".equals(key)) {
            Log.e("SDKDemo", "Quit due to wrong KEY");
            System.exit(0);
        }
    }
    
}
