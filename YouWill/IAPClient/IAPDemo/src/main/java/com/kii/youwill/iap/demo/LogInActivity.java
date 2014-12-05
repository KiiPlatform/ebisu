package com.kii.youwill.iap.demo;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;

/**
 * Created by tian on 13-8-19.
 */
public class LogInActivity extends Activity implements View.OnClickListener {

    private static final boolean TEST_MODE = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.log_in_activity);
        usernameEdit = (EditText) findViewById(R.id.login_email);
        passwordEdit = (EditText) findViewById(R.id.login_password);
        Button b = (Button) findViewById(R.id.btn_login);
        b.setOnClickListener(this);
        b = (Button) findViewById(R.id.btn_cancel);
        b.setOnClickListener(this);
        b.setText("Register");
        callback = new UserCallback();
        if (TEST_MODE) {
            usernameEdit.setText("yw2000@test.com");
            passwordEdit.setText("123123");
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_login:
                KiiUser.logIn(callback, usernameEdit.getText().toString(), passwordEdit.getText().toString());
                dialog = new ProgressDialog(this);
                dialog.setMessage("Logging in...");
                dialog.show();
                break;
            case R.id.btn_cancel:
                startActivityForResult(new Intent(this, RegisterActivity.class), 0);
                break;
        }
    }

    public class UserCallback extends KiiUserCallBack {

        @Override
        public void onLoginCompleted(int arg0, KiiUser arg1, Exception arg2) {
            if (dialog != null) {
                dialog.dismiss();
                dialog = null;
            }
            if (arg2 == null) {
                // Log in success
                Log.d("LogInActivity", "user: " + arg1);
                Log.d("LogInActivity", "user name is " + Kii.user().getUsername() + ", toJson is " + Kii.user()
                        .toJSON() + ", toUri is " + Kii.user().toUri().getLastPathSegment());
                Settings.saveToken(LogInActivity.this, Kii.user().getAccessToken());
                finish();
            } else {
                Log.d("LogInActivity", "Exception: " + arg2.getLocalizedMessage());
            }
        }

    }

    private EditText usernameEdit, passwordEdit;
    private ProgressDialog dialog;
    private UserCallback callback;

}
