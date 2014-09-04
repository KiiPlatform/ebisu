package com.kii.youwill.iap.demo.utils;

import android.content.Context;

import java.io.File;

/**
 * Created by tian on 14-3-4.
 */
public class FileUtils {

    public static File getCacheDir(Context context) {
        File cacheDir = context.getExternalCacheDir();
        if (cacheDir == null) {
            cacheDir = new File("/sdcard/android/data/" + context.getPackageName()
                    + "/cache");
        }
        return cacheDir;

    }


}
