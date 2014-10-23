package com.youwill.store.utils;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import com.youwill.store.providers.YouWill;

import java.util.ArrayList;
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
        ArrayList<ContentValues> valuesArrayList = new ArrayList<ContentValues>();
        for (PackageInfo mInfo : packageInfos) {
            String packageName = mInfo.packageName;
            gLocalApps.put(packageName, mInfo);
            ContentValues value = new ContentValues();
            value.put(YouWill.LocalApps.PACKAGE_NAME, packageName);
            value.put(YouWill.LocalApps.VERSION_CODE, mInfo.versionCode);
            valuesArrayList.add(value);
        }
        ContentValues[] values = valuesArrayList.toArray(new ContentValues[0]);
        ContentResolver mCR = context.getContentResolver();
        mCR.delete(YouWill.LocalApps.CONTENT_URI, null, null);
        mCR.bulkInsert(YouWill.LocalApps.CONTENT_URI, values);
    }
}
