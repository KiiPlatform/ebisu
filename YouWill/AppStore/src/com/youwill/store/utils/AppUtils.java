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
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Created by Evan on 14/10/22.
 */
public class AppUtils {

    public static HashMap<String, PackageInfo> gLocalApps
            = new HashMap<String, PackageInfo>();

    public static void fetchAllPackages(Context context) {
        PackageManager pm = context.getPackageManager();
        List<PackageInfo> packageInfos = pm.getInstalledPackages(0);
        ArrayList<ContentValues> valuesArrayList = new ArrayList<ContentValues>();
        for (PackageInfo info : packageInfos) {
            String packageName = info.packageName;
            gLocalApps.put(packageName, info);
            ContentValues value = new ContentValues();
            value.put(YouWill.LocalApps.PACKAGE_NAME, packageName);
            value.put(YouWill.LocalApps.VERSION_CODE, info.versionCode);
            valuesArrayList.add(value);
        }
        ContentValues[] values = valuesArrayList.toArray(new ContentValues[0]);
        ContentResolver resolver = context.getContentResolver();
        resolver.delete(YouWill.LocalApps.CONTENT_URI, null, null);
        resolver.bulkInsert(YouWill.LocalApps.CONTENT_URI, values);
    }

    public static void updateLocalApp(Context context, String packageName) {
        PackageManager pm = context.getPackageManager();
        try {
            PackageInfo info = pm.getPackageInfo(packageName, 0);
            gLocalApps.put(packageName, info);
            ContentValues value = new ContentValues();
            value.put(YouWill.LocalApps.PACKAGE_NAME, packageName);
            value.put(YouWill.LocalApps.VERSION_CODE, info.versionCode);
            ContentResolver resolver = context.getContentResolver();
            resolver.insert(YouWill.LocalApps.CONTENT_URI, value);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
    }

    public static void deleteLocalApp(Context context, String packageName) {
        gLocalApps.remove(packageName);
        ContentResolver resolver = context.getContentResolver();
        resolver.delete(YouWill.LocalApps.CONTENT_URI, YouWill.LocalApps.PACKAGE_NAME + "=?",
                new String[]{packageName});
    }

    public static void uninstallApp(Context context, String packageName) {
        Uri uri = Uri.parse("package:" + packageName);
        Intent intent = new Intent(Intent.ACTION_UNINSTALL_PACKAGE, uri);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    public static void installApp(Context context, Uri fileUri) {
//        Intent installIntent = new Intent(Intent.ACTION_VIEW)
//                .setDataAndType(fileUri,
//                        "application/vnd.android.package-archive");
//        installIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
//        context.startActivity(installIntent);
        LogUtils.d("installApp, uri is " + fileUri + ", filePath is " + fileUri.getPath());
        silentInstall(fileUri.getPath());
    }

    public static void bindButton(Context context, JSONObject appInfo, Button button) {
        int status = Utils.getStatus(appInfo);
        switch (status) {
            case Utils.APP_STATUS_NONE: {
                double price = appInfo.optDouble("price");
                String priceStr;
                if (price > 0) {
                    boolean isPurchased = appInfo.optBoolean("is_purchased", false);
                    if (isPurchased) {
                        priceStr = context.getString(R.string.download_button);
                    } else {
                        float p = (float) price;
                        priceStr = String.format("￥%.2f", p);
                    }
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
            default:
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
                //fall through
            case DownloadManager.STATUS_FAILED:
                //fall through
            case Utils.APP_STATUS_CAN_UPGRADE:
                //fall through
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
                break;
            case DownloadManager.STATUS_RUNNING:
                break;
            case DownloadManager.STATUS_SUCCESSFUL: {
                DownloadInfo info = DownloadAgent.getInstance().getDownloadProgressMap().get(appId);
                installApp(context, info.fileUri);
            }
            break;
            default:
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

    public static int silentInstall(String filePath) {
        LogUtils.d("silentInstall, filePath is " + filePath);
        File file;
        if (filePath == null || filePath.length() == 0 || (file = new File(filePath)) == null
                || file.length() <= 0
                || !file.exists() || !file.isFile()) {
            return 1;
        }

        String[] args = {"pm", "install", "-r", filePath};
        ProcessBuilder processBuilder = new ProcessBuilder(args);

        Process process = null;
        BufferedReader successResult = null;
        BufferedReader errorResult = null;
        StringBuilder successMsg = new StringBuilder();
        StringBuilder errorMsg = new StringBuilder();
        int result;
        try {
            process = processBuilder.start();
            successResult = new BufferedReader(new InputStreamReader(process.getInputStream()));
            errorResult = new BufferedReader(new InputStreamReader(process.getErrorStream()));
            String s;

            while ((s = successResult.readLine()) != null) {
                successMsg.append(s);
            }

            while ((s = errorResult.readLine()) != null) {
                errorMsg.append(s);
            }
        } catch (IOException e) {
            e.printStackTrace();
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            try {
                if (successResult != null) {
                    successResult.close();
                }
                if (errorResult != null) {
                    errorResult.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
            if (process != null) {
                process.destroy();
            }
        }

        if (successMsg.toString().contains("Success") || successMsg.toString()
                .contains("success")) {
            result = 0;
        } else {
            result = 2;
        }

        Log.d("YouwillInstaller", "successMsg:" + successMsg + ", ErrorMsg:" + errorMsg);

        return result;

    }
}
