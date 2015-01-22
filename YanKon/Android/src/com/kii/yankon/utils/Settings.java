package com.kii.yankon.utils;

import com.kii.yankon.App;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

/**
 * Created by tian on 14-9-17:上午7:43.
 */
public class Settings {

    public static final String PREF_NAME = "preferences";

    private static final String TOKEN_KEY = "token";

    private static final String EXP_KEY = "exp";

    static Context gContext;

    public static SharedPreferences getPrefs() {
        if (gContext == null) {
            gContext = App.getApp();
        }
        int code = Context.MODE_MULTI_PROCESS;
        return gContext.getSharedPreferences(PREF_NAME, code);
    }

    private static final String SHOW_GUIDE = "showGuideV1";

    public static boolean needShowGuide() {
        SharedPreferences pref = getPrefs();
        boolean needShow = pref.getBoolean(SHOW_GUIDE, true);
        if (needShow) {
            SharedPreferences.Editor editor = pref.edit();
            editor.putBoolean(SHOW_GUIDE, false);
            editor.apply();
        }
        return needShow;
    }

    private static final String FIRST_LAUNCH_KEY = "first_launch";

    public static boolean isFirstLaunch() {
        SharedPreferences pref = getPrefs();
        boolean isFirst = pref.getBoolean(FIRST_LAUNCH_KEY, true);
        if (isFirst) {
            SharedPreferences.Editor editor = pref.edit();
            editor.putBoolean(FIRST_LAUNCH_KEY, false);
            editor.apply();
        }
        return isFirst;
    }

    public static void saveToken(String token) {
        getPrefs().edit().putString(TOKEN_KEY, token).apply();
    }

    public static String getToken() {
        return getPrefs().getString(TOKEN_KEY, null);
    }

    public static void saveExp(long t) {
        getPrefs().edit().putLong(EXP_KEY, t).apply();
    }

    public static long getExp() {
        return getPrefs().getLong(EXP_KEY, 0L);
    }

    public static boolean isLoggedIn() {
        return getToken() != null;
    }

    public static final String EMAIL = "email";

    public static void saveEmail(String email) {
        getPrefs().edit().putString(EMAIL, email).apply();
    }

    public static String getEmail() {
        return getPrefs().getString(EMAIL, "");
    }

    public static void logOut() {
        saveEmail(null);
        saveToken(null);
        getPrefs().edit().commit();
    }

    public static final int SERVER_WIN = 0;

    public static final int CLIENT_WIN = 1;

    public static final int BOTH_WIN = 2;

    public static int getWinPolicy() {
        int ret = 0;
        try {
            ret = Integer.parseInt(PreferenceManager.getDefaultSharedPreferences(App.getApp())
                    .getString("win_policy", "0"));
        } catch (Exception e) {
            e.printStackTrace();
        }
        return ret;
    }

    public static boolean isServerWin() {
        return getWinPolicy() == SERVER_WIN;
    }

    public static boolean isBothWin() {
        return getWinPolicy() == BOTH_WIN;
    }

}
