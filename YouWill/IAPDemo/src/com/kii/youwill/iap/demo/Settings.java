package com.kii.youwill.iap.demo;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Created by tian on 13-8-23.
 */
public class Settings {
    private static final String PREFS = "preferences";
    private static final String TOKEN_KEY = "token";
    public static void saveToken(Context context, String token) {
        SharedPreferences pref = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        editor.putString(TOKEN_KEY, token);
        editor.commit();
    }

    public static String getToken(Context context) {
        SharedPreferences pref = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return pref.getString(TOKEN_KEY, "");
    }
}
