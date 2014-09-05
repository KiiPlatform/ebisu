package com.kii.youwill.iap.demo;

import com.kii.youwill.iap.demo.utils.FileUtils;
import com.nostra13.universalimageloader.cache.disc.impl.TotalSizeLimitedDiscCache;
import com.nostra13.universalimageloader.cache.memory.impl.LRULimitedMemoryCache;
import com.nostra13.universalimageloader.core.DisplayImageOptions;
import com.nostra13.universalimageloader.core.ImageLoader;
import com.nostra13.universalimageloader.core.ImageLoaderConfiguration;

import android.app.Application;
import android.content.Context;
import android.view.Display;
import android.view.WindowManager;

public class IAPApplication extends Application {

    private static final int IMAGE_LOADER_THREAD_POOL_SIZE = 3;

    private static final int MEMORY_CACHE_SIZE = 2 * 1024 * 1024;

    private static final int DISC_CACHE_SIZE = 10 * 1024 * 1024;

    @Override
    public void onCreate() {
        // TODO Auto-generated method stub
        super.onCreate();
        WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        Display display = wm.getDefaultDisplay();
        int width = display.getWidth();
        int height = display.getHeight();
        DisplayImageOptions options = new DisplayImageOptions.Builder().cacheInMemory(true)
                .cacheOnDisc(true).build();
        ImageLoaderConfiguration configuration = new ImageLoaderConfiguration.Builder(
                getApplicationContext())
                .defaultDisplayImageOptions(options).memoryCacheExtraOptions(width, height)
                .threadPoolSize(IMAGE_LOADER_THREAD_POOL_SIZE)
                .memoryCache(new LRULimitedMemoryCache(MEMORY_CACHE_SIZE))
                .memoryCacheSize(MEMORY_CACHE_SIZE).discCache(new TotalSizeLimitedDiscCache(
                        FileUtils
                                .getCacheDir
                                        (getApplicationContext()), DISC_CACHE_SIZE))
                .build();
        ImageLoader.getInstance().init(configuration);

    }

}
