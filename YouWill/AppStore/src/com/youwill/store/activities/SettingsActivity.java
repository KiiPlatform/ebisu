package com.youwill.store.activities;

import com.youwill.store.R;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

/**
 * Created by tian on 14/11/13:下午11:18.
 */
public class SettingsActivity extends Activity implements View.OnClickListener{

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().setLayout(585, 556);
    }

    @Override
    public void onClick(View v) {

    }
}
