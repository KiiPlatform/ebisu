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

    private static final long K = 1024;
    private static final long M = K * K;
    private static final long G = M * K;
    private static final long T = G * K;

    public static String getFileSizeString(int size) {
        final long[] dividers = new long[] { T, G, M, K, 1 };
        final String[] units = new String[] { "TB", "GB", "MB", "KB", "B" };
        if(size < 1)
            throw new IllegalArgumentException("Invalid file size: " + size);
        String result = null;
        for(int i = 0; i < dividers.length; i++){
            final long divider = dividers[i];
            if(size >= divider){
                result = format(size, divider, units[i]);
                break;
            }
        }
        return result;
    }

    private static String format(final long value,
            final long divider,
            final String unit){
        final double result =
                divider > 1 ? (double) value / (double) divider : (double) value;
        return String.format("%.1f %s", Double.valueOf(result), unit);
    }
}
