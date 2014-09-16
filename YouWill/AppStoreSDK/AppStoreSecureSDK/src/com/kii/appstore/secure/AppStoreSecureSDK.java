package com.kii.appstore.secure;

import android.content.Context;


public class AppStoreSecureSDK {
    
    public static final int MODE_PRODUCTION = 0;
    public static final int MODE_SANDBOX = 1;
    private static int mode = MODE_PRODUCTION;
    
    
    public static void setMode(int mode) {
        AppStoreSecureSDK.mode = mode; 
    }
    
    public static String getSecureKey(Context context) {
        
        return null;
    }
}
