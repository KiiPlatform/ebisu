package com.youwill.store.activities;

import com.youwill.store.R;
import com.youwill.store.utils.Utils;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;

/**
 * Created by tian on 14/11/13:下午11:19.
 */
public class AboutActivity extends Activity implements View.OnClickListener{

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_about);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().setLayout(585, 556);
        findViewById(R.id.close).setOnClickListener(this);
        findViewById(R.id.check_update).setOnClickListener(this);
        findViewById(R.id.link).setOnClickListener(this);
        TextView version = (TextView)findViewById(R.id.version);
        version.setText(getString(R.string.version_prompt) + Utils.getVersionName(this));
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.close:
                finish();
                break;
            case R.id.check_update:
                //TODO
                break;
            case R.id.link:
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse("http://www.youwill.com.cn"));
                try {
                    startActivity(intent);
                } catch (Exception ignored) {

                }
                break;
            default:
                break;
        }
    }
}
