package com.youwill.store.activities;

import android.app.Activity;
import android.os.Bundle;
import android.view.WindowManager;

import com.youwill.store.R;

public class AppDetailActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_app_detail);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().setLayout(585,556);

    }

}
