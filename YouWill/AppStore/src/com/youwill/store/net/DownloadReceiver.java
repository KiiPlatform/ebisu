package com.youwill.store.net;

import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Settings;

import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

public class DownloadReceiver extends BroadcastReceiver {

    public DownloadReceiver() {
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
        }
    }
}
