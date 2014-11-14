package com.youwill.store.activities;

import com.youwill.store.R;
import com.youwill.store.utils.Settings;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;

/**
 * Created by tian on 14/11/13:下午11:18.
 */
public class SettingsActivity extends Activity implements View.OnClickListener{
    private TextView logInButton;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().setLayout(585, 556);
        findViewById(R.id.clear_cache).setOnClickListener(this);
        findViewById(R.id.close).setOnClickListener(this);
        findViewById(R.id.about).setOnClickListener(this);
        logInButton = (TextView)findViewById(R.id.log_in_button);
        logInButton.setOnClickListener(this);
        if (Settings.isLoggedIn(this)) {
            logInButton.setText(R.string.log_out);
        } else {
            logInButton.setText(R.string.log_in_account);
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.clear_cache:
                //TODO
                break;
            case R.id.close:
                finish();
                break;
            case R.id.about:
                startActivity(new Intent(this, AboutActivity.class));
                break;
            case R.id.log_in_button:
                startActivity(new Intent(this, LogInActivity.class));
                break;
            default:
                break;
        }
    }
}
