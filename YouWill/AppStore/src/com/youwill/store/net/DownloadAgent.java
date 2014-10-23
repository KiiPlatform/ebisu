package com.youwill.store.net;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Utils;

import org.json.JSONObject;

import android.app.DownloadManager;
import android.content.ContentValues;
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

    private DownloadAgent() {
    }

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
        ContentValues cv = new ContentValues(3);
        DownloadManager.Request request = buildDownloadRequest(appId, cv);
        if (request == null) {
            return;
        }
        long id = manager.enqueue(request);
        cv.put(YouWill.Downloads.DOWNLOAD_ID, id);
        cv.put(YouWill.Downloads.APP_ID, appId);
        context.getContentResolver().insert(YouWill.Downloads.CONTENT_URI, cv);
    }

    private DownloadManager.Request buildDownloadRequest(String appId, ContentValues cv) {
        Cursor c = null;
        try {
            c = context.getContentResolver().query(YouWill.Application.CONTENT_URI,
                    new String[]{YouWill.Application.APP_INFO, YouWill.Application.APP_PACKAGE},
                    YouWill.Application.APP_ID + "=?",
                    new String[]{appId}, null);
            if (c != null && c.moveToFirst()) {
                JSONObject app = new JSONObject(c.getString(0));
                String url = app.optString("apk_url");
                String name = app.optString("name");

                DownloadManager.Request request = new DownloadManager.Request(Uri.parse(url));
                request.setAllowedNetworkTypes(DownloadManager.Request.NETWORK_MOBILE
                        | DownloadManager.Request.NETWORK_WIFI);
                MimeTypeMap mimeTypeMap = MimeTypeMap.getSingleton();
                String mimeString = mimeTypeMap
                        .getMimeTypeFromExtension("apk");
                request.setMimeType(mimeString);
                request.setNotificationVisibility(
                        DownloadManager.Request.VISIBILITY_VISIBLE);
                request.setVisibleInDownloadsUi(true);
                cv.put(YouWill.Downloads.PACKAGE_NAME, c.getString(1));
                String filename = c.getString(1) + ".apk";
                request.setDestinationInExternalFilesDir(context, Environment.DIRECTORY_DOWNLOADS,
                        filename);
                request.setTitle(context.getString(R.string.downloading_apk_prompt) + name);
                return request;
            }
        } catch (Exception e) {

        } finally {
            Utils.closeSilently(c);
        }
        return null;
    }
}
