package com.youwill.store.view;

import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.net.DownloadAgent;
import com.youwill.store.net.DownloadInfo;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.AppUtils;
import com.youwill.store.utils.Utils;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.database.Cursor;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CursorAdapter;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.RatingBar;
import android.widget.TextView;


/**
 * Created by Evan on 14/10/23.
 */
public class AppListAdapter extends CursorAdapter implements View.OnClickListener {

    public static final int TYPE_PURCHASED = 0;

    public static final int TYPE_UPGRADE = 1;

    protected Context mContext;

    int mType;

    public AppListAdapter(Context context, Cursor c, int type) {
        super(context, c, CursorAdapter.FLAG_REGISTER_CONTENT_OBSERVER);
        mContext = context;
        mType = type;
    }

    @Override
    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        return LayoutInflater.from(context).inflate(R.layout.app_list_item, parent, false);
    }

    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        String app_info_str = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_INFO));
        JSONObject appInfo = null;
        try {
            appInfo = new JSONObject(app_info_str);
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (appInfo == null) {
            return;
        }
        String appId = appInfo.optString("app_id");
        if (TextUtils.isEmpty(appId)) {
            return;
        }
        TextView tv = (TextView) view.findViewById(R.id.app_grid_pos);
        tv.setText(String.valueOf(cursor.getPosition() + 1));
        ImageView iconView = (ImageView) view.findViewById(R.id.app_grid_icon);
        String iconUrl = appInfo.optString("icon");
        ImageLoader.getInstance().displayImage(iconUrl, iconView, Utils.iconDisplayOptions);
        tv = (TextView) view.findViewById(R.id.app_grid_name);
        tv.setText(appInfo.optString("name"));
        RatingBar bar = (RatingBar) view.findViewById(R.id.app_grid_rate);
        bar.setRating(4);
        Button leftBtn = (Button) view.findViewById(R.id.left_btn);
        Button rightBtn = (Button) view.findViewById(R.id.right_btn);
        ProgressBar progressBar = (ProgressBar) view.findViewById(R.id.app_list_progress);
        progressBar.setVisibility(View.GONE);
        String packageName = appInfo.optString("package", Utils.DUMMY_PACKAGE_NAME);

        PackageInfo packageInfo = AppUtils.gLocalApps.get(packageName);
        if (packageInfo != null) {
            if (mType == TYPE_UPGRADE) {
                leftBtn.setVisibility(View.INVISIBLE);
            } else {
                leftBtn.setVisibility(View.VISIBLE);
            }
            leftBtn.setText(context.getString(R.string.uninstall_button));
            leftBtn.setTag(packageName);
            leftBtn.setOnClickListener(this);
        } else {
            leftBtn.setVisibility(View.INVISIBLE);
        }
        rightBtn.setOnClickListener(this);
        rightBtn.setFocusable(false);
        rightBtn.setFocusableInTouchMode(false);
        rightBtn.setTag(appInfo);
        int status = Utils.getStatus(appInfo);
        switch (status) {
            case Utils.APP_STATUS_NONE:
            case DownloadManager.STATUS_FAILED:
                rightBtn.setText(context.getString(R.string.download_button));
                break;
            case Utils.APP_STATUS_INSTALLED:
                rightBtn.setText(context.getString(R.string.open_button));
                break;
            case Utils.APP_STATUS_CAN_UPGRADE:
                rightBtn.setText(context.getString(R.string.upgrade_button));
                break;
            case DownloadManager.STATUS_PAUSED:
                progressBar.setVisibility(View.VISIBLE);
                rightBtn.setText(context.getString(R.string.resume_button));
                break;
            case DownloadManager.STATUS_PENDING:
            case DownloadManager.STATUS_RUNNING:
                progressBar.setVisibility(View.VISIBLE);
                rightBtn.setText(context.getString(R.string.downloading_button));
                break;
            case DownloadManager.STATUS_SUCCESSFUL:
                rightBtn.setText(context.getString(R.string.install_button));
                break;
        }
        AppUtils.bindProgress(appId, progressBar, status);
    }


    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.left_btn: {
                String packageName = (String) v.getTag();
                AppUtils.uninstallApp(mContext, packageName);
            }
            break;
            case R.id.right_btn: {
                JSONObject appInfo = (JSONObject) v.getTag();
                clickRightButton(appInfo);
            }
            break;
        }
    }

    protected void clickRightButton(JSONObject appInfo) {
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
                Intent LaunchIntent = mContext.getPackageManager()
                        .getLaunchIntentForPackage(packageName);
                mContext.startActivity(LaunchIntent);
            }
            break;
            case DownloadManager.STATUS_PENDING:
            case DownloadManager.STATUS_RUNNING:
                break;
            case DownloadManager.STATUS_SUCCESSFUL: {
                DownloadInfo info = DownloadAgent.getInstance().getDownloadProgressMap().get(appId);
                AppUtils.installApp(mContext, info.fileUri);
            }
            break;
        }
    }
}
