package com.kii.payment;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import com.kii.cloud.storage.Kii;

/**
 * Created by tian on 13-7-23.
 */
public class Utils {
    private static final boolean DEBUG = true;
    private static Context gContext = null;
    
    public static final void log(String tag, String msg) {
        if (DEBUG) {
            android.util.Log.d(tag, msg);
        }
    }
    
    public static void setContextRef(Context context) {
        gContext = context.getApplicationContext();
    }
    
    public static int getResourcesIdentifier(String name, String type) {
        if (gContext == null) {
            return 0;
        }
        return gContext.getResources().getIdentifier(name, type, gContext.getPackageName());
    }
    
    public static int getStringResId(String name) {
        return getResourcesIdentifier(name, "string");
    }
    
    public static String getStringByName(String name) {
        return gContext.getString(getStringResId(name));
    }
    
    static String getDistributionId(Context context) {
        return getDefaultStringValue(Constants.KEY_APP_DISTRIBUTION_ID, "", context);
    }

    private static String getDefaultStringValue(String key, String defaultValue, Context context) {
        ApplicationInfo ai = null;
        try {
            // get the application id and key from the manifest
            ai = context.getPackageManager().getApplicationInfo(context.getPackageName(), PackageManager.GET_META_DATA);
        } catch (Exception ignored) {
        }
        String ret = defaultValue;
        if (ai != null && ai.metaData != null && ai.metaData.containsKey(key)) {
            ret = ai.metaData.getString(key);
        }
        return ret;
    }

    public static String getNotifyUrl() {
        return Constants.PLATFORM_EXTENSION_URL + "alipaynotify/" + Kii.getAppId();
    }
}
