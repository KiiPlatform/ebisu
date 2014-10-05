package com.youwill.store.fragments;

import com.youwill.store.R;
import com.youwill.store.utils.Utils;

import android.app.Fragment;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.CookieSyncManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/**
 * Created by tian on 14-10-5:上午10:22.
 */
public class LogInFragment extends Fragment {

    private WebView webView;

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_log_in, container, false);
        webView = (WebView) view.findViewById(R.id.webview);
        webView.setWebViewClient(new InnerWebViewClient());
        webView.getSettings().setDefaultTextEncodingName("UTF-8");
        webView.getSettings().setUseWideViewPort(true);
        webView.getSettings().setLayoutAlgorithm(WebSettings.LayoutAlgorithm.NARROW_COLUMNS);
        webView.getSettings().setLayoutAlgorithm(WebSettings.LayoutAlgorithm.NARROW_COLUMNS);
        webView.setScrollBarStyle(WebView.SCROLLBARS_INSIDE_OVERLAY);
        CookieSyncManager.createInstance(getActivity());
        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.removeAllCookie();

        return view;
    }

    @Override
    public void onDetach() {
        webView.clearCache(true);
        super.onDetach();
    }

    public void refresh() {
        webView.loadUrl("about:blank");
        LayoutInflater inflater = (LayoutInflater) getActivity().getSystemService(
                Context.LAYOUT_INFLATER_SERVICE);
        webView.loadUrl(getOAuthUrl());
    }

    private static final String REDIRECT_URL = "http://youwill.com";

    private String getOAuthUrl() {
        return null;
    }


    private class InnerWebViewClient extends WebViewClient {

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            return super.shouldOverrideUrlLoading(view, url);
        }

        @Override
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            super.onPageStarted(view, url, favicon);
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            super.onPageFinished(view, url);
        }

        @Override
        public void onReceivedError(WebView view, int errorCode, String description,
                String failingUrl) {
            super.onReceivedError(view, errorCode, description, failingUrl);
        }
    }

    private void handleRedirectUrl(WebView view, String url) {
        Bundle values = Utils.parseUrl(url);

        String error = values.getString("error");
        String error_code = values.getString("error_code");

        Intent intent = new Intent();
        intent.putExtras(values);

        if (error == null && error_code == null) {

            String access_token = values.getString("access_token");
            String expires_time = values.getString("expires_in");
//            setResult(RESULT_OK, intent);
//            new OAuthTask(this).execute(access_token, expires_time);
            //TODO: save OAuth result locally
        } else {
//            Toast.makeText(OAuthActivity.this, getString(R.string.you_cancel_login),
//                    Toast.LENGTH_SHORT).show();
//            finish();
            //TODO: show error
        }

    }

}
