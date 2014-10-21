package com.youwill.store.utils;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;

/**
 * Created by tian on 14-9-16:下午9:30.
 */
public class Settings {

    public static final String PREF_NAME = "preferences";

    static Context gContext;

    @SuppressLint("InlinedApi")
    public static SharedPreferences getPrefs(Context context) {
        if (gContext == null && context != null) {
            gContext = context.getApplicationContext();
        }
        int code = Context.MODE_MULTI_PROCESS;
        return gContext.getSharedPreferences(PREF_NAME, code);
    }

    public static void saveAppKey(Context context, String packageName, String key) {
        SharedPreferences pref = getPrefs(context);
        SharedPreferences.Editor editor = pref.edit();
        editor.putString(packageName, key);
        editor.apply();
    }

    public static String getAppKey(Context context, String packageName) {
        return getPrefs(context).getString(packageName, "");
    }

    public static void saveDownloadId(Context context, long id, String appId) {
        getPrefs(context).edit().putString(Long.toString(id), appId);
    }

    public static String getDownloadApp(Context context, long downloadId) {
        return getPrefs(context).getString(Long.toString(downloadId), "");
    }

    public static void deleteDownloadAppRecord(Context context, long downloadId) {
        getPrefs(context).edit().remove(Long.toString(downloadId));
    }
}
