package com.youwill.store.view;

import android.content.Context;
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

import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.AppUtils;
import com.youwill.store.utils.Utils;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by Evan on 14/10/23.
 */
public class AppListAdapter  extends CursorAdapter {
    public static final int TYPE_PURCHASED = 0;
    public static final int TYPE_UPGRADE = 1;

    int mType;
    public AppListAdapter(Context context, Cursor c, int type) {
        super(context, c, CursorAdapter.FLAG_REGISTER_CONTENT_OBSERVER);
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
        tv = (TextView) view.findViewById(R.id.app_grid_name);
        tv.setText(appInfo.optString("name"));
        RatingBar bar = (RatingBar) view.findViewById(R.id.app_grid_rate);
        bar.setRating(4);
        String packageName = appInfo.optString("package","DUMMY_PACKAGE");
        Button leftBtn = (Button) view.findViewById(R.id.left_btn);
        Button rightBtn = (Button) view.findViewById(R.id.right_btn);
        PackageInfo packageInfo = AppUtils.gLocalApps.get(packageName);
        if (packageInfo != null) {
            leftBtn.setVisibility(View.VISIBLE);
            leftBtn.setText(context.getString(R.string.uninstall_button));
            int versionCode = appInfo.optInt("version_code");
            if (versionCode > packageInfo.versionCode) {
                rightBtn.setText(context.getString(R.string.upgrade_button));
            } else {
                rightBtn.setText(context.getString(R.string.open_button));
            }
        } else {
            leftBtn.setVisibility(View.INVISIBLE);
            rightBtn.setText(context.getString(R.string.download_button));
        }
    }
}
