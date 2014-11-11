package com.youwill.store;

import android.app.Application;
import com.kii.cloud.storage.Kii;
import com.kii.payment.YouWillIAPSDK;
import com.nostra13.universalimageloader.core.ImageLoader;
import com.nostra13.universalimageloader.core.ImageLoaderConfiguration;
import com.youwill.store.net.DownloadAgent;
import com.youwill.store.utils.AppUtils;
import com.youwill.store.utils.DataUtils;
import com.youwill.store.utils.Settings;

/**
 * Created by Evan on 14/10/19.
 */
public class App extends Application {

    private static final int DISK_CACHE_SIZE = 100 * 1024 * 1024;

    @Override
    public void onCreate() {
        super.onCreate();
        final String APP_ID = "c99e04f1";
        Kii.initialize(APP_ID, "3ebdc0472c0c705bc50eaf1756061b8b", Kii.Site.CN);
        YouWillIAPSDK.init("YouWill", APP_ID);

        ImageLoaderConfiguration config = new ImageLoaderConfiguration.Builder(this)
                .diskCacheSize(DISK_CACHE_SIZE)
                .build();
        ImageLoader.getInstance().init(config);
        DownloadAgent.getInstance().init(this);
        new Thread() {
            @Override
            public void run() {
                DownloadAgent.getInstance().loadDownloads();
                AppUtils.fetchAllPackages(App.this);
                if (Settings.isLoggedIn(App.this)) {
                    DataUtils.getPurchasedList(App.this);
                }
            }
        }.start();
    }
}
