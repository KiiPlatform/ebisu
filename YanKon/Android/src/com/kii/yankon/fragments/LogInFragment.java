package com.kii.yankon.fragments;


import android.app.Fragment;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Patterns;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;
import com.kii.yankon.R;
import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.KiiSync;
import com.kii.yankon.utils.Settings;

import butterknife.ButterKnife;
import butterknife.InjectView;

/**
 * A simple {@link Fragment} subclass.
 * create an instance of this fragment.
 */
public class LogInFragment extends BaseFragment implements View.OnClickListener {

    @InjectView(R.id.email_edit)
    EditText emailEdit;

    @InjectView(R.id.password_edit)
    EditText passwordEdit;

    @InjectView(R.id.forgot_password)
    TextView forgetPasswordButton;

    @InjectView(R.id.button_register)
    Button registerButton;

    @InjectView(R.id.button_log_in)
    Button logInButton;

    public LogInFragment() {
        // Required empty public constructor
    }

    public static LogInFragment newInstance(int sectionNumber) {
        LogInFragment fragment = new LogInFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        View view = inflater.inflate(R.layout.fragment_log_in, container, false);
        ButterKnife.inject(this, view);
        forgetPasswordButton.setOnClickListener(this);
        registerButton.setOnClickListener(this);
        logInButton.setOnClickListener(this);
        return view;
    }


    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.forgot_password:
                //TODO
                break;
            case R.id.button_register:
                if (isInputValid()) {
                    KiiUser.builderWithEmail(emailEdit.getText().toString()).build()
                            .register(mUserCallBack, passwordEdit.getText().toString());
                    showProgress();
                }
                break;
            case R.id.button_log_in:
                if (isInputValid()) {
                    KiiUser.logIn(mUserCallBack, emailEdit.getText().toString(),
                            passwordEdit.getText().toString());
                    showProgress();
                }
                break;
            default:
                break;
        }
    }

    private boolean isInputValid() {
        String email = emailEdit.getText().toString();
        if (!TextUtils.isEmpty(email) && Patterns.EMAIL_ADDRESS.matcher(email).matches()) {
            return !TextUtils.isEmpty(passwordEdit.getText().toString());
        }
        return false;
    }

    private KiiUserCallBack mUserCallBack = new KiiUserCallBack() {
        @Override
        public void onTaskCancel(int token) {
            super.onTaskCancel(token);
        }

        @Override
        public void onLoginCompleted(int token, KiiUser user, Exception exception) {
            super.onLoginCompleted(token, user, exception);
            dismissProgress();
            if (exception == null) {
                Toast.makeText(getActivity(), R.string.log_in_success, Toast.LENGTH_SHORT).show();
                Settings.saveToken(user.getAccessToken());
                Settings.saveEmail(user.getEmail());
                LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(new Intent(
                        Constants.INTENT_LOGGED_IN));
                KiiSync.asyncStartSync();
            } else {
                Toast.makeText(getActivity(), exception.getLocalizedMessage(), Toast.LENGTH_SHORT)
                        .show();
            }
        }

        @Override
        public void onRegisterCompleted(int token, KiiUser user, Exception exception) {
            super.onRegisterCompleted(token, user, exception);
            dismissProgress();
            if (exception == null) {
                Toast.makeText(getActivity(), R.string.register_success, Toast.LENGTH_SHORT).show();
                long expires_at = user.getAccessTokenBundle().getLong("expires_at");
                Settings.saveToken(user.getAccessToken());
                Settings.saveEmail(user.getEmail());
                Settings.saveExp(expires_at);
                LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(new Intent(
                        Constants.INTENT_LOGGED_IN));
                KiiSync.asyncStartSync();
            } else {
                Toast.makeText(getActivity(), exception.getLocalizedMessage(), Toast.LENGTH_SHORT)
                        .show();
            }
        }
    };
}