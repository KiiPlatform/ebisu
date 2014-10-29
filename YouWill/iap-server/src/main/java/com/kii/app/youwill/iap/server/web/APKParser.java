package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.appstore.APKParserService;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.json.JSONException;
import org.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/**
 * Created by liangyx on 10/13/14.
 */

@RestController
@RequestMapping(value="/util")
public class APKParser {

    @Autowired
    private APKParserService apkParser;

    @RequestMapping(value="/apkparser",produces="application/json")
    public String parseAPK(@RequestBody String body) {
        String url = null;
        String appID = null;
        try {
            JSONObject json = new JSONObject(body);
            url = json.optString("apk_url");
            appID = json.optString("app_id");
        } catch (JSONException e) {
            e.printStackTrace();
        }

        if (url == null || url.length() == 0) {
            throw new ServiceException(IAPErrorCode.REQUEST_BODY_INVALID);
        }

        System.out.println("apk_url: " + url);
        return apkParser.parseAPK(appID, url);
    }

}
