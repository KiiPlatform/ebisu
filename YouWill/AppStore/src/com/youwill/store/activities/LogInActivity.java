package com.youwill.store.activities;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiSocialCallBack;
import com.kii.cloud.storage.social.KiiSocialConnect;
import com.kii.cloud.storage.social.connector.KiiSocialNetworkConnector;
import com.youwill.store.net.NetworkConstants;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Settings;

import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.HttpClient;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.message.BasicNameValuePair;
import org.apache.http.util.EntityUtils;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

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
                    Settings.setNick(LogInActivity.this, user.getUsername());
                }
                if (exception != null) {
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
//        getUserData();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == KiiSocialNetworkConnector.REQUEST_CODE && resultCode == RESULT_OK) {
            KiiSocialConnect connect = Kii.socialConnect(
                    KiiSocialConnect.SocialNetwork.SOCIALNETWORK_CONNECTOR);
            connect.respondAuthOnActivityResult(requestCode, resultCode, data);
            Bundle bundle = connect.getAccessTokenBundle();
            for (String key : bundle.keySet()) {
                LogUtils.d("onActivityResult, key is " + key + ", value is " + bundle.get(key)
                        .toString());
            }
            Settings.setYouWillToken(this, bundle.getString("oauth_token"));
            Settings.setYouWillId(this, bundle.getString("provider_user_id"));
            getUserData();
        } else {
            finish();
        }
    }

    private void getUserData() {
        new Thread() {
            @Override
            public void run() {
                super.run();
                try {
                    getUserInfo();
                } catch (IOException e) {
                    mHandler.sendEmptyMessage(MSG_GET_DATA_SUCCESS);
                } catch (JSONException e) {
                    e.printStackTrace();
                    mHandler.sendEmptyMessage(MSG_GET_DATA_SUCCESS);
                }
            }
        }.start();
    }

    private void getUserInfo() throws IOException, JSONException {
        HttpClient client = new DefaultHttpClient();
        HttpPost request = new HttpPost(NetworkConstants.USER_INFO_URL);
        List<BasicNameValuePair> pairs = new ArrayList<BasicNameValuePair>();
        BasicNameValuePair pair = new BasicNameValuePair("access_token",
                Settings.getYouWillToken(this));
        pairs.add(pair);
        UrlEncodedFormEntity entity = new UrlEncodedFormEntity(pairs, "utf-8");
        request.setEntity(entity);
        HttpResponse resp = client.execute(request);
        if (resp.getStatusLine().getStatusCode() == HttpStatus.SC_OK) {
            String result = EntityUtils.toString(resp.getEntity(), "utf-8");
            LogUtils.d("getUserInfo, result is " + result);
            JSONObject object = new JSONObject(result);
            JSONObject data = object.optJSONObject("data");
            if (data != null && object.optBoolean("valid")) {
                Settings.setYouWillToken(this, object.optString("new_access_token"));
                Settings.setNick(this, data.optString("NickName"));
            }
        }
        mHandler.sendEmptyMessage(MSG_GET_DATA_SUCCESS);
    }

    private static final int MSG_GET_DATA_SUCCESS = 0;

    private static final int MSG_BEGIN_GETTING = 1;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_GET_DATA_SUCCESS:
                    finish();
                    break;
                case MSG_BEGIN_GETTING:
                    break;
                default:
                    break;
            }
        }
    };
}
