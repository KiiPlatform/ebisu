package com.youwill.store;

import com.kii.cloud.storage.Kii;
import com.nostra13.universalimageloader.core.ImageLoader;
import com.nostra13.universalimageloader.core.ImageLoaderConfiguration;
import com.youwill.store.net.DownloadAgent;

import android.app.Application;

/**
 * Created by Evan on 14/10/19.
 */
public class App extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        Kii.initialize("c99e04f1", "3ebdc0472c0c705bc50eaf1756061b8b", Kii.Site.CN);
        ImageLoaderConfiguration config = new ImageLoaderConfiguration.Builder(this)
                .build();
        ImageLoader.getInstance().init(config);
        DownloadAgent.getInstance().init(this);
    }
}
