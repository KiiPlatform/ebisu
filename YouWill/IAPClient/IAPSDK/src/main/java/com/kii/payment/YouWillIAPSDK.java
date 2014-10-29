package com.kii.payment;

import com.kii.cloud.storage.Kii;

/**
 * Class to perform initialization.
 *
 * Created by Evan on 14-9-3.
 */
public class YouWillIAPSDK {
    static {
        Kii.initialize("c99e04f1", "3ebdc0472c0c705bc50eaf1756061b8b", Kii.Site.CN);
    }

    private static String gYouWillAppId = null;

    private static String gAuthorId = null;

    /**
     * Set the authorID and the appID
     *
     * @param authorId
     * @param appId
     */
    public static void init(String authorId, String appId) {
        gAuthorId = authorId;
        gYouWillAppId = appId;
    }

    public static String getYouWillAppID() {
        return gYouWillAppId;
    }

    public static String getAuthorID() {
        return gAuthorId;
    }
}
