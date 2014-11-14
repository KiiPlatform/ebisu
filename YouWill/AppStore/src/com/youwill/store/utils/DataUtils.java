package com.youwill.store.utils;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import android.util.Log;
import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;
import com.kii.cloud.storage.query.KiiQueryResult;
import com.kii.payment.KiiReceipt;
import com.kii.payment.KiiStore;
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
                    editor.apply();
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
                int version_code = obj.getInt("version_code");
                String searchField = obj.getString("name") + ";" + obj.getString("description");
                values.put(YouWill.Application.APP_ID, appId);
                values.put(YouWill.Application.PACKAGE_NAME, packageName);
                values.put(YouWill.Application.APP_INFO, info);
                values.put(YouWill.Application.RECOMMEND_TYPE, recommendType);
                values.put(YouWill.Application.RECOMMEND_WEIGHT, recommendWeight);
                values.put(YouWill.Application.SEARCH_FIELD, searchField);
                values.put(YouWill.Application.AGE_CATEGORY, category);
                values.put(YouWill.Application.VERSION_CODE, version_code);
                valuesArrayList.add(values);
            }
        }
        if (valuesArrayList.size() > 0) {
            ContentValues[] values = valuesArrayList
                    .toArray(new ContentValues[valuesArrayList.size()]);
            context.getContentResolver().bulkInsert(YouWill.Application.CONTENT_URI, values);
        }
    }

    public static void getPurchasedList(final Context context) {
        KiiUser currentUser = KiiUser.getCurrentUser();
        if (currentUser != null && currentUser.isLoggedIn()) {
            refreshPurchasedList(context, currentUser);
        } else {
            KiiUser.loginWithToken(new KiiUserCallBack() {
                @Override
                public void onLoginCompleted(int token, final KiiUser user, Exception exception) {
                    if (exception == null) {
                        new Thread() {
                            @Override
                            public void run() {
                                refreshPurchasedList(context, user);
                            }
                        }.start();

                    } else {
                        LogUtils.d("loginWithToken failed with exception: " + exception);
                    }
                }
            }, Settings.getToken(context));
        }
    }

    private static void refreshPurchasedList(Context context, KiiUser user) {
        LogUtils.d("refreshPurchasedList.");
        if ((user == null) || (context == null)) {
            LogUtils.d("refreshPurchasedList, user:" + user + "; context: " + context);
            return;
        }

        List<KiiReceipt> receipts = KiiStore.listReceipts(null, user);

        ContentResolver cr = context.getContentResolver();
        cr.delete(YouWill.Purchased.CONTENT_URI, null, null);
        List<ContentValues> values = new ArrayList<ContentValues>();
        int i = 0;
        for (KiiReceipt receipt : receipts) {
            ContentValues v = new ContentValues();
            String app = receipt.getFieldByName("app");
            if (app == null) {
                continue;
            }
            v.put("app_id", app);
            values.add(v);
        }

        cr.bulkInsert(YouWill.Purchased.CONTENT_URI, values.toArray(new ContentValues[values.size()]));
    }

    public static void appendPurchasedApp(Context context, String appID) {
        ContentResolver cr = context.getContentResolver();
        ContentValues v = new ContentValues();
        v.put("app_id", appID);
        cr.insert(YouWill.Purchased.CONTENT_URI, v);
    }

}
