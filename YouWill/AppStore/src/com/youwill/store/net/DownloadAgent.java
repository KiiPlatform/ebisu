package com.youwill.store.net;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Settings;
import com.youwill.store.utils.Utils;

import org.json.JSONObject;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.webkit.MimeTypeMap;

/**
 * Created by tian on 14/10/19:下午1:46.
 */
public class DownloadAgent {

    private Context context;

    private static DownloadAgent instance;

    public static DownloadAgent getInstance() {
        if (instance == null) {
            synchronized (DownloadAgent.class) {
                if (instance == null) {
                    instance = new DownloadAgent();
                }
            }
        }
        return instance;
    }

    public void init(Context context) {
        this.context = context;
    }

    public void beginDownload(String appId) {
        DownloadManager manager = (DownloadManager) context
                .getSystemService(Context.DOWNLOAD_SERVICE);
        DownloadManager.Request request = buildDownloadRequest(appId);
        long id = manager.enqueue(request);
        Settings.saveDownloadId(context, id, appId);
    }

    private DownloadManager.Request buildDownloadRequest(String appId) {
        Cursor c = null;
        try {
            c = context.getContentResolver().query(YouWill.Application.CONTENT_URI,
                    new String[]{YouWill.Application.APP_INFO}, YouWill.Application.APP_ID + "=?",
                    new String[]{appId}, null);
            if (c != null && c.moveToFirst()) {
                JSONObject app = new JSONObject(c.getString(0));
                String url = app.optString("apk_url");
                String name = app.optString("name");
                String icon = app.optString("icon");
                int size = app.optInt("size");
                DownloadManager.Request request = new DownloadManager.Request(Uri.parse(url));
                request.setAllowedNetworkTypes(DownloadManager.Request.NETWORK_MOBILE
                        | DownloadManager.Request.NETWORK_WIFI);
                MimeTypeMap mimeTypeMap = MimeTypeMap.getSingleton();
                String mimeString = mimeTypeMap
                        .getMimeTypeFromExtension(MimeTypeMap.getFileExtensionFromUrl(url));
                request.setMimeType(mimeString);
                request.setNotificationVisibility(
                        DownloadManager.Request.VISIBILITY_VISIBLE);
                request.setVisibleInDownloadsUi(true);
                request.setDestinationInExternalFilesDir(context, Environment.DIRECTORY_DOWNLOADS,
                        "apk");
                request.setTitle(context.getString(R.string.downloading_apk_prompt) + name);
            }
        } catch (Exception e) {

        } finally {
            Utils.closeSilently(c);
        }
        return null;
    }
}
