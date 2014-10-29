package com.kii.app.youwill.iap.server.appstore;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import net.dongliu.apk.parser.ApkParser;
import net.dongliu.apk.parser.bean.ApkMeta;
import net.dongliu.apk.parser.bean.UseFeature;
import org.codehaus.jettison.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import org.springframework.stereotype.Controller;

import java.io.*;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.util.*;

/**
 * Created by liangyx on 10/13/14.
 */
@Component
public class APKParserService {

    @Value("${temp_dir}")
    String mTempDir;

    public String parseAPK(String appID, String urlString) {
        try {
            URL url = new URL(urlString);
            URLConnection connection = url.openConnection();
            InputStream inputStream = connection.getInputStream();
            File apkFile = downloadAPK(inputStream, appID);
            if (apkFile == null) {
                throw new ServiceException(IAPErrorCode.CANNOT_DOWNLOAD_APK);
            }

            ApkParser apkParser = new ApkParser(apkFile);
            ApkMeta meta = apkParser.getApkMeta();

            JSONObject appInfo = new JSONObject();
            appInfo.put("name", meta.getLabel());
            appInfo.put("package", meta.getPackageName());
            appInfo.put("version_code", meta.getVersionCode());
            appInfo.put("version_name", meta.getVersionName());
            appInfo.put("minsdk", meta.getMinSdkVersion());
            appInfo.put("apk_size", apkFile.length());
            List<String> permissions = meta.getPermissions();
            if (permissions != null) {
                appInfo.put("use_permissions", permissions);
            }

            List<UseFeature> features = meta.getUseFeatures();
            if (features != null) {
                List<Map<String, Object>> list = new ArrayList<Map<String, Object>>();
                for (UseFeature feature : features) {
                    HashMap<String, Object> map = new HashMap<String, Object>();
                    map.put("name", feature.getName());
                    map.put("is_required", feature.isRequired());
                    list.add(map);
                }
                appInfo.put("use_features", list);
            }
            //System.out.println("isLargeScreens: " + meta.isLargeScreens());
            //System.out.println("isNormalScreens: " + meta.isNormalScreens());
            //System.out.println("isSmallScreens: " + meta.isSmallScreens());
            //System.out.println("isAnyDensity: " + meta.isAnyDensity());
            //System.out.println("isHasNative: " + meta.isHasNative());
            JSONObject response = new JSONObject();
            response.put("errorCode", 0);
            response.put("app_meta", appInfo);
            return response.toString();
        } catch (MalformedURLException e) {
            e.printStackTrace();
            throw new ServiceException(IAPErrorCode.REQUEST_BODY_INVALID);
        } catch (IOException e) {
            throw new ServiceException(IAPErrorCode.UNKNOWN_INTERNAL_ERROR);
        } catch (JSONException e) {
            throw new ServiceException(IAPErrorCode.UNKNOWN_INTERNAL_ERROR);
        }
    }

    private File downloadAPK(InputStream input, String appID) {
        BufferedInputStream bis = new BufferedInputStream(input);
        FileOutputStream fos = null;
        String fileName = "app_" + appID + "_" + System.currentTimeMillis() + ".apk";
        File tmpFile = new File(mTempDir);
        if (!tmpFile.exists()) {
            tmpFile.mkdirs();
        }

        File destFile = new File(mTempDir, fileName);

        byte[] buffer = new byte[1024 * 4];
        try {
            fos = new FileOutputStream(destFile);
            int len;
            while ((len = bis.read(buffer)) != -1) {
                fos.write(buffer, 0, len);
            }
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        } finally {
            safeClose(fos);
            safeClose(bis);
            safeClose(input);
        }
        return destFile;
    }

    private void safeClose(Closeable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
}
