package com.youwill.store.utils;

import com.youwill.store.R;
import com.youwill.store.net.DownloadAgent;
import com.youwill.store.net.DownloadInfo;
import com.youwill.store.providers.YouWill;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.DownloadManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Created by Evan on 14/10/22.
 */
public class AppUtils {

    public static final HashMap<String, PackageInfo> gLocalApps
            = new HashMap<String, PackageInfo>();

    public static void fetchAllPackages(Context context) {
        PackageManager pm = context.getPackageManager();
        List<PackageInfo> packageInfos = pm.getInstalledPackages(0);
        ArrayList<ContentValues> valuesArrayList = new ArrayList<ContentValues>();
        for (PackageInfo mInfo : packageInfos) {
            String packageName = mInfo.packageName;
            gLocalApps.put(packageName, mInfo);
            ContentValues value = new ContentValues();
            value.put(YouWill.LocalApps.PACKAGE_NAME, packageName);
            value.put(YouWill.LocalApps.VERSION_CODE, mInfo.versionCode);
            valuesArrayList.add(value);
        }
        ContentValues[] values = valuesArrayList.toArray(new ContentValues[0]);
        ContentResolver mCR = context.getContentResolver();
        mCR.delete(YouWill.LocalApps.CONTENT_URI, null, null);
        mCR.bulkInsert(YouWill.LocalApps.CONTENT_URI, values);
    }

    public static void uninstallApp(Context context, String packageName) {
        Uri uri = Uri.parse("package:" + packageName);
        Intent intent = new Intent(Intent.ACTION_UNINSTALL_PACKAGE, uri);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    public static void installApp(Context context, Uri fileUri) {
        Intent installIntent = new Intent(Intent.ACTION_VIEW)
                .setDataAndType(fileUri,
                        "application/vnd.android.package-archive");
        installIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(installIntent);
    }

    public static void bindButton(Context context, JSONObject appInfo, Button button) {
        int status = Utils.getStatus(appInfo);
        switch (status) {
            case Utils.APP_STATUS_NONE: {
                int price = appInfo.optInt("price");
                String priceStr;
                if (price > 0) {
                    float p = (float) price;
                    priceStr = String.format("￥%.2f", p);
                } else {
                    priceStr = context.getString(R.string.price_free);
                }
                button.setText(priceStr);
            }
            break;
            case DownloadManager.STATUS_FAILED:
                button.setText(context.getString(R.string.download_button));
                break;
            case Utils.APP_STATUS_INSTALLED:
                button.setText(context.getString(R.string.open_button));
                break;
            case Utils.APP_STATUS_CAN_UPGRADE:
                button.setText(context.getString(R.string.upgrade_button));
                break;
            case DownloadManager.STATUS_PAUSED:
                button.setText(context.getString(R.string.resume_button));
                break;
            case DownloadManager.STATUS_PENDING:
            case DownloadManager.STATUS_RUNNING:
                button.setText(context.getString(R.string.downloading_button));
                break;
            case DownloadManager.STATUS_SUCCESSFUL:
                button.setText(context.getString(R.string.install_button));
                break;
        }
    }

    public static void clickPriceButton(Context context, JSONObject appInfo) {
        int status = Utils.getStatus(appInfo);
        String appId;
        try {
            appId = appInfo.getString("app_id");
        } catch (JSONException e) {
            return;
        }
        switch (status) {
            case Utils.APP_STATUS_NONE:
            case DownloadManager.STATUS_FAILED:
            case Utils.APP_STATUS_CAN_UPGRADE:
            case DownloadManager.STATUS_PAUSED:
                DownloadAgent.getInstance().beginDownload(appId);
                break;
            case Utils.APP_STATUS_INSTALLED: {
                String packageName = appInfo.optString("package", Utils.DUMMY_PACKAGE_NAME);
                Intent LaunchIntent = context.getPackageManager()
                        .getLaunchIntentForPackage(packageName);
                context.startActivity(LaunchIntent);
            }
            break;
            case DownloadManager.STATUS_PENDING:
            case DownloadManager.STATUS_RUNNING:
                break;
            case DownloadManager.STATUS_SUCCESSFUL: {
                DownloadInfo info = DownloadAgent.getInstance().getDownloadProgressMap().get(appId);
                installApp(context, info.fileUri);
            }
            break;
        }
    }

    public static void bindProgress(String appId, ProgressBar progressBar, int status) {
        if (status == DownloadManager.STATUS_PAUSED || status == DownloadManager.STATUS_PENDING
                || status == DownloadManager.STATUS_RUNNING) {
            progressBar.setVisibility(View.VISIBLE);
        } else {
            progressBar.setVisibility(View.GONE);
        }
        DownloadInfo info = DownloadAgent.getInstance().getDownloadProgressMap().get(appId);
        if (info != null) {
            progressBar.setProgress(info.percentage);
        }
    }
}
