package com.youwill.store.utils;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.query.KiiQueryResult;
import com.youwill.store.providers.YouWill;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import android.util.Log;

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
        new Thread() {
            @Override
            public void run() {
                try {
                    KiiQueryResult<KiiObject> result = Kii.bucket("apps")
                            .query(null);
                    List<KiiObject> objLists = result.getResult();
                    storeAppsToDB(context, objLists);
                    while (result.hasNext()) {
                        result = result.getNextQueryResult();
                        objLists = result.getResult();
                        storeAppsToDB(context, objLists);
                    }
                    SharedPreferences pref = Settings.getPrefs(context);
                    SharedPreferences.Editor editor = pref.edit();
                    editor.putLong(KEY_LAST_GET_APPS_TIME, System.currentTimeMillis());
                    editor.commit();
                } catch (Exception e) {
                    LogUtils.e(LOG_TAG, Log.getStackTraceString(e));
                }
            }
        }.start();
    }

    private static void storeAppsToDB(Context context, List<KiiObject> objLists) {
        ArrayList<ContentValues> valuesArrayList = new ArrayList<ContentValues>();
        for (KiiObject obj : objLists) {
            String appId = obj.getString("app_id");
            String packageName = obj.getString("package");
            String info = obj.toString();
            if (!TextUtils.isEmpty(appId) && !TextUtils.isEmpty(packageName)) {
                ContentValues values = new ContentValues();
                int recommendType = obj.getInt("recommend_type", -1);
                int recommendWeight = obj.getInt("recommend_weight", -1);
                int category = obj.getInt("category_id", -1);
                String searchField = obj.getString("name") + ";" + obj.getString("description");
                values.put(YouWill.Application.APP_ID, appId);
                values.put(YouWill.Application.APP_PACKAGE, packageName);
                values.put(YouWill.Application.APP_INFO, info);
                values.put(YouWill.Application.RECOMMEND_TYPE, recommendType);
                values.put(YouWill.Application.RECOMMEND_WEIGHT, recommendWeight);
                values.put(YouWill.Application.SEARCH_FIELD, searchField);
                values.put(YouWill.Application.AGE_CATEGORY, category);
                valuesArrayList.add(values);
            }
        }
        if (valuesArrayList.size() > 0) {
            ContentValues[] values = valuesArrayList.toArray(new ContentValues[0]);
            context.getContentResolver().bulkInsert(YouWill.Application.CONTENT_URI, values);
        }
    }

    public static void getPurchasedList(final Context context) {
        ContentResolver cr = context.getContentResolver();
        cr.delete(YouWill.Purchased.CONTENT_URI, null, null);
        ContentValues[] values = new ContentValues[7];
        for (int i = 0; i < 7; i++) {
            ContentValues v = new ContentValues();
            v.put("app_key", "test");
            values[i] = v;
        }
        values[0].put("app_id", "f363bc5c");
        values[1].put("app_id", "a7e0a105");
        values[2].put("app_id", "1fbe85a3");
        values[3].put("app_id", "255ab961");
        values[4].put("app_id", "58ec3c91");
        values[5].put("app_id", "471f6a0a");
        values[6].put("app_id", "a6b9f9ed");
        cr.bulkInsert(YouWill.Purchased.CONTENT_URI, values);
    }

}
