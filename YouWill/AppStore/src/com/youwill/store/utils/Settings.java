package com.youwill.store.utils;

import com.kii.payment.PayType;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

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

    public static void saveDownloadId(Context context, long id, String appId) {
        getPrefs(context).edit().putString(Long.toString(id), appId);
    }

    public static String getDownloadApp(Context context, long downloadId) {
        return getPrefs(context).getString(Long.toString(downloadId), "");
    }

    public static void deleteDownloadAppRecord(Context context, long downloadId) {
        getPrefs(context).edit().remove(Long.toString(downloadId));
    }

    public static final String USER_ID_KEY = "user_id";

    public static void setUserId(Context context, String userId) {
        getPrefs(context).edit().putString(USER_ID_KEY, userId).apply();
    }

    public static String getUserId(Context context) {
        return getPrefs(context).getString(USER_ID_KEY, "");
    }

    public static final String TOKEN_KEY = "token";

    public static void setToken(Context context, String token) {
        getPrefs(context).edit().putString(TOKEN_KEY, token).apply();
    }

    public static String getToken(Context context) {
        return getPrefs(context).getString(TOKEN_KEY, "");
    }

    public static boolean isLoggedIn(Context context) {
        if (TextUtils.isEmpty(getToken(context)) || TextUtils.isEmpty(getUserId(context))) {
            return false;
        }
        return true;
    }

    public static final String PAY_TYPE_KEY = "pay_type";

    public static void setLastUsedPayType(Context context, PayType payment) {
        getPrefs(context).edit().putString(PAY_TYPE_KEY, payment.name()).apply();
    }

    public static PayType getLastUsedPayType(Context context) {
        String payType = getPrefs(context).getString(PAY_TYPE_KEY, null);
        if (payType == null) {
            return PayType.alipay;
        } else {
            return PayType.valueOf(payType);
        }
    }


    public static final String NICK = "nick";

    public static void setNick(Context context, String nick) {
        getPrefs(context).edit().putString(NICK, nick).apply();
    }

    public static String getNick(Context context) {
        return getPrefs(context).getString(NICK, "YouWill用户");
    }

    public static void registerListener(Context context,
                                        SharedPreferences.OnSharedPreferenceChangeListener listener) {
        getPrefs(context).registerOnSharedPreferenceChangeListener(listener);
    }

    public static void unregisterListener(Context context,
                                          SharedPreferences.OnSharedPreferenceChangeListener listener) {
        getPrefs(context).unregisterOnSharedPreferenceChangeListener(listener);
    }

    public static void logOut(Context context) {
        getPrefs(context).edit().clear().commit();
    }
}
