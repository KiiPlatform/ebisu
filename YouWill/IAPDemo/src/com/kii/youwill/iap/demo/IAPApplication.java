package com.kii.youwill.iap.demo;

import android.app.Application;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.Kii.Site;

public class IAPApplication extends Application {

    @Override
    public void onCreate() {
        // TODO Auto-generated method stub
        super.onCreate();
        Kii.initialize("c99e04f1", "3ebdc0472c0c705bc50eaf1756061b8b", Site.CN);
    }

}
