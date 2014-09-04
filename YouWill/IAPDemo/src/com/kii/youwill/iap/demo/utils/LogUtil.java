package com.kii.youwill.iap.demo.utils;

/**
 * Created by tian on 3/4/14.
 */
public class LogUtil {

    private static final boolean DEBUG = true;

    public static final void log(String tag, String msg) {
        if (DEBUG) {
            android.util.Log.d(tag, msg);
        }
    }
}
