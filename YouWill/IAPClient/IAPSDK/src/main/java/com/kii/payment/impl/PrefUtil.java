package com.kii.payment.impl;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by liangyx on 9/11/14.
 */
public class PrefUtil {
    private static final String PREFS = "youwillpay";

    private static final String ORDER_ID = "order_id";
    private static final String RETRY = "retry";


    private static void saveClientResult(Context context, String productId, String transactionID) {
        SharedPreferences pref = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        editor.putString(productId, transactionID);
        editor.commit();
    }

    private static String getClientResult(Context context, String productId) {
        SharedPreferences pref = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return pref.getString(productId, null);
    }

    private static void removeClientResult(Context context, String productId) {
        SharedPreferences pref = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        editor.remove(productId);
        editor.commit();
    }

    public static void cacheClientPaymentStatus(Context context, String productID, String orderID) {
        String result = PrefUtil.getClientResult(context, productID);
        JSONObject jsonObject = null;
        try {
            if (TextUtils.isEmpty(result)) {
                jsonObject = new JSONObject();
                jsonObject.put(ORDER_ID, orderID);
                jsonObject.put(RETRY, 1);
            } else {
                jsonObject = new JSONObject(result);
                jsonObject.put(RETRY, jsonObject.getInt(RETRY) + 1);
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }

        saveClientResult(context, productID, jsonObject.toString());
    }

    public static void clearCachedResult(Context context, String productID) {
        removeClientResult(context, productID);
    }

    public static String getCachedTransactionID(Context context, String productID) {
        String result = getClientResult(context, productID);

        if (TextUtils.isEmpty(result)) {
            return null;
        }
        try {
            JSONObject jsonObject = new JSONObject(result);
            return jsonObject.getString(ORDER_ID);

        } catch (JSONException e) {
            e.printStackTrace();
            return null;
        }
    }

    public static int getRetryNum(Context context, String productID) {
        String result = getClientResult(context, productID);

        if (TextUtils.isEmpty(result)) {
            return 0;
        }
        try {
            JSONObject jsonObject = new JSONObject(result);
            return jsonObject.getInt(RETRY);

        } catch (JSONException e) {
            e.printStackTrace();
            return 0;
        }
    }
}
