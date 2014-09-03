package com.kii.payment;

import com.kii.cloud.storage.Kii;

/**
 * Created by Evan on 14-9-3.
 */
public class YouWillIAPSDK {
    static {
        Kii.initialize("c99e04f1", "3ebdc0472c0c705bc50eaf1756061b8b", Kii.Site.CN);
    }

    protected static String gYouWillAppId = null;

    public static void init(String appId) {
        gYouWillAppId = appId;
    }
}
