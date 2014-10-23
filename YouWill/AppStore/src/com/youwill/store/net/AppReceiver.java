package com.youwill.store.net;

import com.youwill.store.utils.LogUtils;

import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Environment;

import java.io.File;

public class AppReceiver extends BroadcastReceiver {

    public AppReceiver() {
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals(DownloadManager.ACTION_DOWNLOAD_COMPLETE)) {
            long downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1);
            LogUtils.d("onReceive, download id is " + downloadId);
            DownloadManager manager = (DownloadManager) context
                    .getSystemService(Context.DOWNLOAD_SERVICE);
            Uri uri = manager.getUriForDownloadedFile(downloadId);
            Intent installIntent = new Intent(Intent.ACTION_VIEW)
                    .setDataAndType(uri,
                            "application/vnd.android.package-archive");
            installIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(installIntent);
        } else if (intent.getAction().equals(Intent.ACTION_PACKAGE_ADDED)) {
            String packageName = intent.getData().getSchemeSpecificPart();
            LogUtils.d("onReceive, package name is " + packageName);
            try {
                //the package has been installed, delete the downloaded package;
                String filename = packageName + ".apk";
                File file = new File(
                        context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS), filename);
                file.delete();
            } catch (Exception ignored) {

            }
        }
    }
}
