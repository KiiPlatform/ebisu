package com.kii.app.youwill.iap.server.mm;


import net.sf.json.JSONObject;
import org.springframework.util.DigestUtils;

/**
 * Created by liangyx on 12/1/14.
 */
public class MMUtil {

    public static boolean verifyRequest(JSONObject syncRequest) {
        //OrderID# ChannelID#PayCode#AppKey
        String orderID = syncRequest.optString("OrderID", "");
        String channelID = syncRequest.optString("ChannelID", "");
        String payCode = syncRequest.optString("PayCode", "");
        String appID = syncRequest.optString("AppID", "");
        String appKey = getAppKey(appID);
        String original = String.format("%s#%s#%s#%s", orderID, channelID, payCode, appKey);
        String md5Hex = DigestUtils.md5DigestAsHex(original.getBytes()).toUpperCase();
        String md5Sign = syncRequest.optString("MD5Sign", "");
        return md5Sign.equals(md5Hex);
    }

    public static String getAppKey(String appID) {
        return MMConfig.APP_KEY;
    }
}
