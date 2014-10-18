package com.youwill.store.utils;

import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import android.util.Log;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.callback.KiiQueryCallBack;
import com.kii.cloud.storage.query.KiiQuery;
import com.kii.cloud.storage.query.KiiQueryResult;
import com.youwill.store.providers.AppProvider;
import com.youwill.store.providers.YouWill;


import java.util.ArrayList;
import java.util.List;

/**
 * Created by Evan on 14/10/18.
 */
public class DataUtils {
    private static final String LOG_TAG = "DataUtils";
    private static final String KEY_LAST_GET_APPS_TIME = "last_apps_timestamp";
    private static final long GET_APPS_TIME_WINDOW = 20 * 60 * 1000; // 20 minutes

    public static void loadApps(final Context context) {
        long last_time = Settings.getPrefs(context).getLong(KEY_LAST_GET_APPS_TIME, 0);
        if (System.currentTimeMillis() - last_time < GET_APPS_TIME_WINDOW) {
            return;
        }
        KiiQuery all_query = new KiiQuery();
        Kii.bucket("apps").query(new KiiQueryCallBack<KiiObject>() {
            @Override
            public void onQueryCompleted(int token, KiiQueryResult<KiiObject> result, Exception exception) {
                if (exception != null) {
                    // Error handling
                    Log.e(LOG_TAG, Log.getStackTraceString(exception));
                    return;
                }

                List<KiiObject> objLists = result.getResult();
                storeAppsToDB(context, objLists);

                while (result.hasNext()) {
                    try {
                        result = result.getNextQueryResult();
                    } catch (Exception e) {
                        Log.e(LOG_TAG, Log.getStackTraceString(exception));
                        break;
                    }
                    objLists = result.getResult();
                    storeAppsToDB(context, objLists);
                }

                SharedPreferences pref = Settings.getPrefs(context);
                SharedPreferences.Editor editor = pref.edit();
                editor.putLong(KEY_LAST_GET_APPS_TIME, System.currentTimeMillis());
                editor.commit();
            }
        }, all_query);
    }

    private static void storeAppsToDB(Context context, List<KiiObject> objLists) {
        ArrayList<ContentValues> valuesArrayList = new ArrayList<ContentValues>();
        for (KiiObject obj : objLists) {
            String appId = obj.getString("app_id");
            String packageName = obj.getString("package");
            String info = obj.toString();
            if (!TextUtils.isEmpty(appId) && !TextUtils.isEmpty(packageName)) {
                ContentValues values = new ContentValues();
                values.put(YouWill.Application.APP_ID, appId);
                values.put(YouWill.Application.APP_PACKAGE, packageName);
                values.put(YouWill.Application.APP_INFO, info);
                valuesArrayList.add(values);
            }
        }
        if (valuesArrayList.size() > 0) {
            ContentValues[] values = valuesArrayList.toArray(new ContentValues[0]);
            context.getContentResolver().bulkInsert(YouWill.Application.CONTENT_URI, values);
        }
    }

}
