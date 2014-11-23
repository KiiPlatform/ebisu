package com.kii.yankon;

import android.app.Application;

/**
 * Created by Evan on 14/11/23.
 */
public class YanKonApplication extends Application{

    private static YanKonApplication sInstance;
    public static YanKonApplication getInstance() {
        return sInstance;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        sInstance = this;
    }
}
