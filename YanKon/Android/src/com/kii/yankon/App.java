package com.kii.yankon;

import android.app.Application;
import android.content.ContentValues;

import com.kii.cloud.storage.Kii;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.Global;

import java.util.HashMap;

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
        Kii.initialize("06e806e2", "31afdcdfd72ade025559176a40a20875", Kii.Site.JP);
        Global.gLightsIpMap = new HashMap<>();
        Global.gLightsMacMap = new HashMap<>();
        ContentValues values = new ContentValues();
        values.put("connected", false);
        getContentResolver().update(YanKonProvider.URI_LIGHTS, values, null, null);
    }
}
