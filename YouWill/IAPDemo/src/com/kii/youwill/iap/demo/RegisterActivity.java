package com.kii.youwill.iap.demo;

import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;

import android.app.Activity;
import android.app.ProgressDialog;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.EditText;

/**
 * Created by tian on 13-8-19.
 */
public class RegisterActivity extends Activity implements View.OnClickListener {
    private EditText usernameEdit, passwordEdit;
    private ProgressDialog dialog = null;
    private static final String TAG = RegisterActivity.class.getName();
    private UserCallback callback;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.register_activity);
        View v = findViewById(R.id.btn_login);
        v.setOnClickListener(this);
        v = findViewById(R.id.btn_cancel);
        v.setOnClickListener(this);
        usernameEdit = (EditText) findViewById(R.id.register_email);
        passwordEdit = (EditText) findViewById(R.id.register_password);
        callback = new UserCallback();
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_login:
                KiiUser user = KiiUser.builderWithEmail(usernameEdit.getText().toString()).build();
                user.register(callback, passwordEdit.getText().toString());
                if (dialog == null) {
                    dialog = new ProgressDialog(this);
                }
                dialog.setTitle("Registering, please wait");
                dialog.setIndeterminate(true);
                dialog.show();
                break;
            case R.id.btn_cancel:
                setResult(RESULT_CANCELED);
                finish();
                break;
        }
    }

    public class UserCallback extends KiiUserCallBack {

        @Override
        public void onRegisterCompleted(int token, KiiUser user, Exception ex) {
            Log.d(TAG, "onRegisterCompleted, token is " + token + ", user is " + user
                    .getEmail() + ", ex is " + ex + ", accessToken is " + user.getAccessToken());
            if (ex == null) {
                KiiUser.loginWithToken(callback, String.valueOf(user.getAccessToken()));
                setResult(RESULT_OK);
                finish();
            } else {
                // TODO: show error
                try {
                    if (dialog != null) {
                        dialog.dismiss();
                        dialog = null;
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }

        @Override
        public void onLoginCompleted(int token, KiiUser user, Exception ex) {
            Log.d(TAG, "onLoginCompleted, token is " + token + ", user is " + user + ", ex is " + ex);
            try {
                if (dialog != null) {
                    dialog.dismiss();
                    dialog = null;
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
            setResult(0);
            finish();
        }

    }


}
