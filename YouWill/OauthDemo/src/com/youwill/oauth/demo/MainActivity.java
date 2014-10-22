package com.youwill.oauth.demo;

import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.AsyncTask;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

import java.io.UnsupportedEncodingException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

public class MainActivity extends Activity {

    private WebView webview;

    private InnerWebViewClient client;

    private MenuItem refreshItem;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        webview = (WebView) findViewById(R.id.webview);
        client = new InnerWebViewClient();
        webview.setWebViewClient(client);

        WebSettings settings = webview.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setSaveFormData(false);
        settings.setCacheMode(WebSettings.LOAD_NO_CACHE);

        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.removeAllCookie();
    }

    private class InnerWebViewClient extends WebViewClient {

        @Override
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            Log.d(TAG, "onPageStarted, url is " + url);
//            if (url.startsWith(Constants.REDIRECT_URL)) {
//                handleRedirectUrl(view, url);
//                view.stopLoading();
//                return;
//            }
            super.onPageStarted(view, url, favicon);
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            Log.d(TAG, "shouldOverrideUrlLoading? " + url);
            view.loadUrl(url);
            return true;
        }
    }

    private static final String TAG = MainActivity.class.getCanonicalName();

    private void handleRedirectUrl(WebView view, String url) {
        Log.d(TAG, "handleRedirectUrl, url is " + url);
        Bundle values = parseUrl(url);

        String error = values.getString("error");
        String error_code = values.getString("error_code");

        Intent intent = new Intent();
        intent.putExtras(values);

        if (error == null && error_code == null) {
            String code = values.getString("code");
            String state = values.getString("state");
            setResult(RESULT_OK, intent);
            new OAuthTask().execute(code, state);
        } else {
            Toast.makeText(this, getString(R.string.you_cancel_login),
                    Toast.LENGTH_SHORT).show();
            finish();
        }

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        webview.clearCache(true);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.actionbar_menu_oauthactivity, menu);
        refreshItem = menu.findItem(R.id.menu_refresh);
        refresh();
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_refresh:
                refresh();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    public void refresh() {
        webview.stopLoading();
        webview.loadUrl("about:blank");
        webview.loadUrl(getOAuthUrl());
    }

    private String getOAuthUrl() {
        Map<String, String> parameters = new HashMap<String, String>();
        parameters.put("client_id", Constants.CLIENT_ID);
        parameters.put("response_type", "code");
        parameters.put("redirect_uri", Constants.REDIRECT_URL);
        return Constants.OAUTH_URL; //+ "?" + encodeUrl(parameters) + "&state=foobar";
    }

    public static Bundle parseUrl(String url) {
        try {
            URL u = new URL(url);
            Bundle b = decodeUrl(u.getQuery());
            b.putAll(decodeUrl(u.getRef()));
            return b;
        } catch (MalformedURLException e) {
            return new Bundle();
        }
    }

    public static Bundle decodeUrl(String s) {
        Bundle params = new Bundle();
        if (s != null) {
            String array[] = s.split("&");
            for (String parameter : array) {
                String v[] = parameter.split("=");
                try {
                    params.putString(URLDecoder.decode(v[0], "UTF-8"),
                            URLDecoder.decode(v[1], "UTF-8"));
                } catch (UnsupportedEncodingException e) {
                    e.printStackTrace();

                }
            }
        }
        return params;
    }

    public static String encodeUrl(Map<String, String> param) {
        if (param == null) {
            return "";
        }

        StringBuilder sb = new StringBuilder();

        Set<String> keys = param.keySet();
        boolean first = true;

        for (String key : keys) {
            String value = param.get(key);
            if (!TextUtils.isEmpty(value) || key.equals("description") || key.equals("url")) {
                if (first) {
                    first = false;
                } else {
                    sb.append("&");
                }
                try {
                    sb.append(URLEncoder.encode(key, "UTF-8")).append("=")
                            .append(URLEncoder.encode(param.get(key), "UTF-8"));
                } catch (UnsupportedEncodingException e) {

                }
            }
        }
        return sb.toString();
    }

    private class OAuthTask extends AsyncTask<String, Void, Void> {

        public OAuthTask() {
            super();
            showProgress();
        }

        @Override
        protected Void doInBackground(String... params) {
            String url = Constants.OAUTH_URL + "token.html";
            Map<String, String> data = new LinkedHashMap<String, String>();
            data.put("client_id", Constants.CLIENT_ID);
            data.put("client_secret", Constants.CLIENT_SECRET);
            data.put("grant_type", "authorization_code");
            data.put("redirect_uri", Constants.REDIRECT_URL);
            data.put("code", params[0]);
            String body = encodeUrl(data);
            Log.d(TAG, "url is " + url);
            Log.d(TAG, "body is " + body);
            HttpClient client = new DefaultHttpClient();
            HttpPost post = new HttpPost(url);
            try {
                post.setEntity(new StringEntity(body));
                HttpResponse response = client.execute(post);
                Log.d(TAG, "response code is " + response.getStatusLine().getStatusCode());
                String result = EntityUtils.toString(response.getEntity());
                Log.d(TAG, "result is " + result);
            } catch (Exception e) {
                e.printStackTrace();
            }
            return null;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            showProgress();
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            super.onPostExecute(aVoid);
            dismissProgress();
        }
    }

    protected ProgressDialog progressDialog;

    private void initProgressDialog() {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(this);
        }
        progressDialog.setCancelable(true);
    }

    protected void showProgress() {
        initProgressDialog();
        progressDialog.show();
    }

    protected void dismissProgress() {
        if (progressDialog != null) {
            progressDialog.dismiss();
            progressDialog = null;
        }
    }

}
