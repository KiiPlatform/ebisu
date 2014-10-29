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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CursorAdapter;
import android.widget.ImageView;
import android.widget.RatingBar;
import android.widget.TextView;

/**
 * Created by Evan on 14/10/19.
 */
public class AppGridAdapter extends CursorAdapter implements View.OnClickListener {

    protected Context mContext;

    public AppGridAdapter(Context context, Cursor c, int flags) {
        super(context, c, flags);
        mContext = context;
    }

    @Override
    public View newView(Context context, Cursor cursor, ViewGroup viewGroup) {
        return LayoutInflater.from(context).inflate(R.layout.app_grid_item, viewGroup, false);
    }

    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        String app_info_str = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_INFO));
        JSONObject appInfo = null;
        try {
            appInfo = new JSONObject(app_info_str);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        if (appInfo == null) {
            return;
        }
        TextView tv = (TextView) view.findViewById(R.id.app_grid_pos);
        tv.setText(String.valueOf(cursor.getPosition() + 1));
        ImageView iconView = (ImageView) view.findViewById(R.id.app_grid_icon);
        String iconUrl = appInfo.optString("icon");
        ImageLoader.getInstance().displayImage(iconUrl, iconView, Utils.iconDisplayOptions);
        Button price_btn = (Button) view.findViewById(R.id.app_grid_price);
        price_btn.setTag(appInfo);
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
                price_btn.setText(priceStr);
            }
            break;
            case DownloadManager.STATUS_FAILED:
                price_btn.setText(context.getString(R.string.download_button));
                break;
            case Utils.APP_STATUS_INSTALLED:
                price_btn.setText(context.getString(R.string.open_button));
                break;
            case Utils.APP_STATUS_CAN_UPGRADE:
                price_btn.setText(context.getString(R.string.upgrade_button));
                break;
            case DownloadManager.STATUS_PAUSED:
                price_btn.setText(context.getString(R.string.resume_button));
                break;
            case DownloadManager.STATUS_PENDING:
            case DownloadManager.STATUS_RUNNING:
                price_btn.setText(context.getString(R.string.downloading_button));
                break;
            case DownloadManager.STATUS_SUCCESSFUL:
                price_btn.setText(context.getString(R.string.install_button));
                break;
        }

        final String appId = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_ID));
        price_btn.setOnClickListener(this);
        price_btn.setFocusable(false);
        price_btn.setFocusableInTouchMode(false);
        tv = (TextView) view.findViewById(R.id.app_grid_name);
        tv.setText(appInfo.optString("name"));
        RatingBar bar = (RatingBar) view.findViewById(R.id.app_grid_rate);
        bar.setRating(4);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.app_grid_price:
                clickPriceButton((JSONObject) v.getTag());
                break;
        }
    }

    protected void clickPriceButton(JSONObject appInfo) {
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
                Intent LaunchIntent = mContext.getPackageManager().getLaunchIntentForPackage(packageName);
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
