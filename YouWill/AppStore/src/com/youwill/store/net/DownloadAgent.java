package com.youwill.store.net;

import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Constants;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Utils;

import org.json.JSONObject;

import android.app.DownloadManager;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.os.Handler;
import android.support.v4.content.LocalBroadcastManager;
import android.webkit.MimeTypeMap;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by tian on 14/10/19:下午1:46.
 */
public class DownloadAgent {

    private Context context;

    private static DownloadAgent instance;

    private DownloadAgent() {
        mDownloadProgressMap = new HashMap<String, DownloadInfo>();
        mIdMap = new HashMap<Long, String>();
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

    public long beginDownload(String appId) {
        DownloadManager manager = (DownloadManager) context
                .getSystemService(Context.DOWNLOAD_SERVICE);
        ContentValues cv = new ContentValues(3);
        DownloadManager.Request request = buildDownloadRequest(appId, cv);
        if (request == null) {
            return -1;
        }
        long id = manager.enqueue(request);
        cv.put(YouWill.Downloads.DOWNLOAD_ID, id);
        cv.put(YouWill.Downloads.APP_ID, appId);
        context.getContentResolver().insert(YouWill.Downloads.CONTENT_URI, cv);
        registerDownloadId(appId, id);
        return id;
    }

    public void loadDownloads() {
        Cursor c = context.getContentResolver().query(YouWill.Downloads.CONTENT_URI,
                new String[]{YouWill.Downloads.APP_ID, YouWill.Downloads.DOWNLOAD_ID}, null, null,
                null);
        while (c.moveToNext()) {
            String appId = c.getString(0);
            long downloadId = c.getLong(1);
            registerDownloadId(appId, downloadId);
        }
        Utils.closeSilently(c);
    }

    public void removeDownloadAfterInstall(String packageName) {
        Cursor c = context.getContentResolver().query(YouWill.Downloads.CONTENT_URI,
                new String[]{YouWill.Downloads.APP_ID, YouWill.Downloads.DOWNLOAD_ID},
                YouWill.Downloads.PACKAGE_NAME + "=(?)",
                new String[]{packageName},
                null);
        String appId = null;
        if (c.moveToFirst()) {
            appId = c.getString(0);
            long downloadId = c.getLong(1);
            DownloadManager manager = (DownloadManager) context
                    .getSystemService(Context.DOWNLOAD_SERVICE);
            manager.remove(downloadId);
        }
        Utils.closeSilently(c);
        if (appId != null) {
            mIdMap.remove(appId);
            context.getContentResolver().delete(YouWill.Downloads.CONTENT_URI,
                    YouWill.Downloads.APP_ID + "=(?)",
                    new String[]{appId});
        }
    }

    private void registerDownloadId(String appId, long id) {
        Uri uri = Uri
                .withAppendedPath(Uri.parse("content://downloads/my_downloads"), Long.toString(id));
        context.getContentResolver().registerContentObserver(uri, false, mObserver);
        mIdMap.put(id, appId);
        updateDownloadInfo(id);
    }

    private DownloadManager.Request buildDownloadRequest(String appId, ContentValues cv) {
        Cursor c = null;
        try {
            c = context.getContentResolver().query(YouWill.Application.CONTENT_URI,
                    new String[]{YouWill.Application.APP_INFO, YouWill.Application.PACKAGE_NAME},
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
                request.setTitle(name);
                return request;
            }
        } catch (Exception e) {

        } finally {
            Utils.closeSilently(c);
        }
        return null;
    }

    private Map<String, DownloadInfo> mDownloadProgressMap;

    private Map<Long, String> mIdMap;

    public Map<String, DownloadInfo> getDownloadProgressMap() {
        return mDownloadProgressMap;
    }

    private static final String TAG = DownloadAgent.class.getName();

    private ContentObserver mObserver = new ContentObserver(new Handler()) {

        @Override
        public void onChange(boolean selfChange, Uri uri) {
            LogUtils.d(TAG, "onChange, uri is " + uri);
            long id;
            try {
                id = Long.parseLong(uri.getLastPathSegment());
            } catch (Exception e) {
                return;
            }
            updateDownloadInfo(id);
        }
    };

    private void updateDownloadInfo(long downloadId) {
        DownloadManager.Query query = new DownloadManager.Query();
        query.setFilterById(downloadId);
        DownloadManager manager = (DownloadManager) context
                .getSystemService(Context.DOWNLOAD_SERVICE);
        Cursor cursor = manager.query(query);
        if (cursor.moveToFirst()) {
            String appId = mIdMap.get(downloadId);
            DownloadInfo info = mDownloadProgressMap.get(appId);
            if (info == null) {
                info = new DownloadInfo();
            }
            int total = cursor
                    .getInt(cursor.getColumnIndex(DownloadManager.COLUMN_TOTAL_SIZE_BYTES));
            int current = cursor
                    .getInt(cursor.getColumnIndex(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
            info.status = cursor
                    .getInt(cursor.getColumnIndex(DownloadManager.COLUMN_STATUS));
            if (info.status == DownloadManager.STATUS_SUCCESSFUL) {
                info.fileUri = Uri.parse(cursor
                        .getString(cursor.getColumnIndex(DownloadManager.COLUMN_LOCAL_URI)));
            }
            int percentage = 0;
            if (total != 0) {
                percentage = (int) ((float) current / (float) total * 100);
            }
            info.percentage = percentage;
            mDownloadProgressMap.put(appId, info);
            LogUtils.d(TAG, "onProgressUpdate, update " + appId + " to " + percentage);
            LocalBroadcastManager.getInstance(context)
                    .sendBroadcast(new Intent(Constants.INTENT_DOWNLOAD_PROGRESS_CHANGED));
        }
        Utils.closeSilently(cursor);
    }
}
