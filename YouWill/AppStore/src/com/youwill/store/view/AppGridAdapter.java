package com.youwill.store.view;

import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.net.DownloadAgent;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Utils;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
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
public class AppGridAdapter extends CursorAdapter {

    public AppGridAdapter(Context context, Cursor c, int flags) {
        super(context, c, flags);
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
        int price = appInfo.optInt("price");
        String priceStr;
        if (price > 0) {
            float p = (float)price;
            priceStr = String.format("￥%.2f",p);
        } else {
            priceStr = context.getString(R.string.price_free);
        }
        price_btn.setText(priceStr);
        final String appId = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_ID));
        price_btn.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                DownloadAgent.getInstance().beginDownload(appId);
            }
        });
        tv = (TextView) view.findViewById(R.id.app_grid_name);
        tv.setText(appInfo.optString("name"));
        RatingBar bar = (RatingBar) view.findViewById(R.id.app_grid_rate);
        bar.setRating(4);
    }
}
