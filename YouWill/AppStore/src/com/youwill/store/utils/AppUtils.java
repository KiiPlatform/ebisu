package com.youwill.store.utils;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import java.util.HashMap;
import java.util.List;

/**
 * Created by Evan on 14/10/22.
 */
public class AppUtils {
    public static final HashMap<String, PackageInfo> gLocalApps = new HashMap<String, PackageInfo>();
    public static void fetchAllPackages(Context context) {
        PackageManager pm = context.getPackageManager();
        List<PackageInfo> packageInfos = pm.getInstalledPackages(0);
        for (PackageInfo mInfo:packageInfos) {
            String packageName = mInfo.packageName;
            gLocalApps.put(packageName, mInfo);
        }
    }
}
