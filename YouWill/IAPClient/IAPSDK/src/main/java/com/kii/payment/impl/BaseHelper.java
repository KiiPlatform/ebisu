/*
 * Copyright (C) 2010 The MobileSecurePay Project
 * All right reserved.
 * author: shiqun.shi@alipay.com
 */

package com.kii.payment.impl;

import org.json.JSONObject;

public class BaseHelper {

    /**
     * Alipay return value to json
     *
     * @param str
     * @param split
     * @return
     */
    public static JSONObject string2JSON(String str, String split) {
        JSONObject json = new JSONObject();
        try {
            String[] arrStr = str.split(split);
            for (int i = 0; i < arrStr.length; i++) {
                String[] arrKeyValue = arrStr[i].split("=");
                json.put(arrKeyValue[0], arrStr[i].substring(arrKeyValue[0].length() + 1));
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        return json;
    }
}