package com.youwill.store.activities;

import android.app.Activity;
import android.database.Cursor;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.TextView;

import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Utils;

import org.json.JSONObject;

public class AppDetailActivity extends Activity {

    public static final String EXTRA_APPID = "appId";

    String mAppId;
    JSONObject mAppInfo;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_app_detail);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().setLayout(585, 556);
        mAppId = getIntent().getStringExtra(EXTRA_APPID);
        if (TextUtils.isEmpty(mAppId)) {
            finish();
            return;
        }

        Cursor c = getContentResolver().query(YouWill.Application.CONTENT_URI,
                null,
                YouWill.Application.APP_ID + "=(?)",
                new String[]{mAppId},
                null);
        if (c != null && c.moveToFirst()) {
            String app_info_str = c.getString(c.getColumnIndex(YouWill.Application.APP_INFO));
            try {
                mAppInfo = new JSONObject(app_info_str);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        Utils.closeSilently(c);

        if (mAppInfo == null) {
            finish();
            return;
        }
        initViews();
    }

    protected void initViews() {
        ImageView iconView = (ImageView) findViewById(R.id.app_detail_icon);
        String iconUrl = mAppInfo.optString("icon");
        ImageLoader.getInstance().displayImage(iconUrl, iconView, Utils.iconDisplayOptions);
        TextView tv = (TextView) findViewById(R.id.app_detail_name);
        tv.setText(mAppInfo.optString("name"));
    }

}
