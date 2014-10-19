package com.youwill.store.net;

import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Settings;
import com.youwill.store.utils.Utils;

import org.json.JSONObject;

import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.text.TextUtils;

import java.io.File;

public class AppReceiver extends BroadcastReceiver {

    public AppReceiver() {
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals(DownloadManager.ACTION_DOWNLOAD_COMPLETE)) {
            long downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1);
            LogUtils.d("onReceive, download id is " + downloadId);
            String appId = Settings.getDownloadApp(context, downloadId);
            if (TextUtils.isEmpty(appId)) {
                //not my download id, do nothing
            } else {
                DownloadManager manager = (DownloadManager) context
                        .getSystemService(Context.DOWNLOAD_SERVICE);
                Uri uri = manager.getUriForDownloadedFile(downloadId);
                Intent installIntent = new Intent(Intent.ACTION_VIEW)
                        .setDataAndType(uri,
                                "application/vnd.android.package-archive");
                installIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(installIntent);
            }
        } else if (intent.getAction().equals(Intent.ACTION_PACKAGE_ADDED)) {
            String packageName = intent.getData().getSchemeSpecificPart();
            Cursor c = null;
            try {
                c = context.getContentResolver().query(YouWill.Application.CONTENT_URI,
                        new String[]{YouWill.Application.APP_INFO},
                        YouWill.Application.APP_PACKAGE + "=?", new String[]{packageName}, null);
                if (c != null && c.moveToFirst()) {
                    //the package has been installed, delete the downloaded package;
                    JSONObject app = new JSONObject(c.getString(0));
                    String url = app.optString("apk_url");
                    String filename = url.substring(url.lastIndexOf('/') + 1, url.length());
                    File file = new File(
                            context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS), filename);
                    file.delete();
                }
            } catch (Exception e) {

            } finally {
                Utils.closeSilently(c);
            }
        }
    }
}
