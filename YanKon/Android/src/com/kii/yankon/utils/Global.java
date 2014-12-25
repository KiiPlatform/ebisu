package com.kii.yankon.utils;

import com.kii.yankon.model.Light;

import java.util.HashMap;

/**
 * Created by Evan on 14/11/23.
 */
public class Global {

    public static String MY_IP = null;
    public static String BROADCAST_IP = null;
    public static HashMap<String, Light> gLightsMacMap;
    public static HashMap<String, Light> gLightsIpMap;

    public static boolean isWifiConnected = true;
}
