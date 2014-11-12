package com.youwill.store.activities;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiSocialCallBack;
import com.kii.cloud.storage.social.KiiSocialConnect;
import com.kii.cloud.storage.social.connector.KiiSocialNetworkConnector;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Settings;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * Created by tian on 14/10/22:下午10:51.
 */
public class LogInActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        KiiSocialConnect connect = Kii
                .socialConnect(KiiSocialConnect.SocialNetwork.SOCIALNETWORK_CONNECTOR);
        Bundle options = new Bundle();
        options.putParcelable("provider", KiiSocialNetworkConnector.Provider.YOUWILL);
        connect.logIn(this, options, new KiiSocialCallBack() {
            @Override
            public void onLoginCompleted(KiiSocialConnect.SocialNetwork network, KiiUser user,
                    Exception exception) {
                super.onLoginCompleted(network, user, exception);
                if (user != null) {
                    LogUtils.d("onLogInComplete, user is " + user.toJSON());
                    Settings.setUserId(LogInActivity.this, user.getID());
                    Settings.setToken(LogInActivity.this, user.getAccessToken());
                }
                if (exception!=null) {
                    exception.printStackTrace();
                }
            }

            @Override
            public void onLinkCompleted(KiiSocialConnect.SocialNetwork network, KiiUser user,
                    Exception exception) {
                super.onLinkCompleted(network, user, exception);
            }

            @Override
            public void onUnLinkCompleted(KiiSocialConnect.SocialNetwork network, KiiUser user,
                    Exception exception) {
                super.onUnLinkCompleted(network, user, exception);
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == KiiSocialNetworkConnector.REQUEST_CODE && resultCode == RESULT_OK) {
            Kii.socialConnect(KiiSocialConnect.SocialNetwork.SOCIALNETWORK_CONNECTOR)
                    .respondAuthOnActivityResult(
                            requestCode,
                            resultCode,
                            data);
        }
        finish();
    }
}
