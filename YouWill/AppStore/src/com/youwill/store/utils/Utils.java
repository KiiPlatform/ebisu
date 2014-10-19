package com.youwill.store.utils;

import com.nostra13.universalimageloader.core.DisplayImageOptions;

import android.os.Bundle;

import java.io.Closeable;

/**
 * Created by tian on 14-10-5:上午11:03.
 */
public class Utils {

    public static final DisplayImageOptions iconDisplayOptions = new DisplayImageOptions.Builder()
            .cacheInMemory(true)
            .cacheOnDisk(true)
            .build();

    public static Bundle parseUrl(String url) {
        return null;
    }

    public static void closeSilently(Closeable closeable) {
        try {
            if (closeable!=null) {
                closeable.close();
            }
        } catch (Exception e) {

        }
    }
}
