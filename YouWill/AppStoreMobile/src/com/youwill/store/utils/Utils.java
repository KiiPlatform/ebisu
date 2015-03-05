package com.youwill.store.utils;

import com.nostra13.universalimageloader.core.DisplayImageOptions;
import com.youwill.store.R;
import com.youwill.store.fragments.ProgressDialogFragment;
import com.youwill.store.net.DownloadAgent;
import com.youwill.store.net.DownloadInfo;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.DialogFragment;
import android.app.FragmentTransaction;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Bundle;

import java.io.Closeable;

/**
 * Created by tian on 14-10-5:上午11:03.
 */
public class Utils {

    public static final int APP_STATUS_NONE = 0;

    public static final int APP_STATUS_INSTALLED = -1;

    public static final int APP_STATUS_CAN_UPGRADE = -2;

    public static final String DUMMY_PACKAGE_NAME = "Dummy_package_name";

    public static final DisplayImageOptions iconDisplayOptions = new DisplayImageOptions.Builder()
            .showImageOnLoading(R.drawable.default_app_icon)
            .showImageOnFail(R.drawable.default_app_icon)
            .showImageForEmptyUri(R.drawable.default_app_icon)
            .cacheInMemory(true)
            .cacheOnDisk(true)
            .build();

    public static final DisplayImageOptions coverFlowDisplayOptions = new DisplayImageOptions.Builder()
            .showImageOnLoading(R.drawable.cover_flow3)
            .showImageOnFail(R.drawable.cover_flow3)
            .showImageForEmptyUri(R.drawable.cover_flow3)
            .cacheInMemory(true)
            .cacheOnDisk(true)
            .build();

    public static final DisplayImageOptions detailDisplayOptions = new DisplayImageOptions.Builder()
            .showImageOnLoading(R.drawable.default_content_image)
            .showImageOnFail(R.drawable.default_content_image)
            .showImageForEmptyUri(R.drawable.default_content_image)
            .cacheInMemory(true)
            .cacheOnDisk(true)
            .build();

    public static Bundle parseUrl(String url) {
        return null;
    }

    public static void closeSilently(Closeable closeable) {
        try {
            if (closeable != null) {
                closeable.close();
            }
        } catch (Exception e) {

        }
    }

    private static final long K = 1024;

    private static final long M = K * K;

    private static final long G = M * K;

    private static final long T = G * K;

    public static String getFileSizeString(int size) {
        final long[] dividers = new long[]{T, G, M, K, 1};
        final String[] units = new String[]{"TB", "GB", "MB", "KB", "B"};
        if (size < 1) {
            throw new IllegalArgumentException("Invalid file size: " + size);
        }
        String result = null;
        for (int i = 0; i < dividers.length; i++) {
            final long divider = dividers[i];
            if (size >= divider) {
                result = format(size, divider, units[i]);
                break;
            }
        }
        return result;
    }

    private static String format(final long value,
            final long divider,
            final String unit) {
        final double result =
                divider > 1 ? (double) value / (double) divider : (double) value;
        return String.format("%.1f %s", Double.valueOf(result), unit);
    }


    public static int getStatus(JSONObject appInfo) {
        String packageName = appInfo.optString("package", DUMMY_PACKAGE_NAME);
        PackageInfo packageInfo = AppUtils.gLocalApps.get(packageName);
        boolean needDownload = false;
        int status = APP_STATUS_NONE;
        if (packageInfo != null) {
            int versionCode = appInfo.optInt("version_code");
            if (versionCode > packageInfo.versionCode) {
                needDownload = true;
                status = APP_STATUS_CAN_UPGRADE;
            } else {
                status = APP_STATUS_INSTALLED;
            }
        } else {
            needDownload = true;
            status = APP_STATUS_NONE;
        }
        if (needDownload) {
            String appId;
            try {
                appId = appInfo.getString("app_id");
            } catch (JSONException e) {
                return status;
            }
            DownloadInfo info = DownloadAgent.getInstance().getDownloadProgressMap().get(appId);
            if (info != null) {
                status = info.status;
            }
        }
        return status;
    }

    public static boolean isNetworkAvailable(Context context) {
        ConnectivityManager connectivityManager = (ConnectivityManager) context
                .getSystemService(Context.CONNECTIVITY_SERVICE);
        try {
            NetworkInfo info = connectivityManager.getActiveNetworkInfo();
            if (info != null) {
                return info.isConnected();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }

    private static DialogFragment mProgressDialog = null;

    public static void showProgressDialog(Activity activity, String title) {
        mProgressDialog = ProgressDialogFragment.newInstance(title);
        //mProgressDialog.show(activity.getFragmentManager(), "dialog");
        FragmentTransaction ft = activity.getFragmentManager().beginTransaction();
        ft.add(mProgressDialog, "dialog");
        ft.commitAllowingStateLoss();
    }

    public static void dismissProgressDialog() {
        if (mProgressDialog != null) {
            mProgressDialog.dismiss();
        }
    }

    public static String getVersionName(Context context) {
        PackageManager pm = context.getPackageManager();
        String versionName = "Unknown";
        try {
            PackageInfo pi = pm.getPackageInfo(context.getPackageName(), 0);
            versionName = pi.versionName;
        } catch (PackageManager.NameNotFoundException e) {
        }
        return versionName;
    }

    public static int getVersionCode(Context context) {
        PackageManager pm = context.getPackageManager();
        int versionCode = 0;
        try {
            PackageInfo pi = pm.getPackageInfo(context.getPackageName(), 0);
            versionCode = pi.versionCode;
        } catch (PackageManager.NameNotFoundException e) {
        }
        return versionCode;
    }

}
