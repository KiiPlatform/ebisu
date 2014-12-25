package com.youwill.store.activities;

import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;
import com.kii.payment.KiiOrder;
import com.kii.payment.KiiPayment;
import com.kii.payment.KiiPaymentCallback;
import com.kii.payment.KiiProduct;
import com.kii.payment.KiiStore;
import com.kii.payment.PayType;
import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.AppUtils;
import com.youwill.store.utils.Constants;
import com.youwill.store.utils.DataUtils;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Settings;
import com.youwill.store.utils.Utils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.text.format.DateFormat;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.RatingBar;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

public class AppDetailActivity extends Activity
        implements View.OnClickListener {

    public static final String EXTRA_APP_ID = "appId";

    String mAppId;

    JSONObject mAppInfo;

    RecyclerView mRecyclerView;

    LinearLayoutManager mLinearLayoutManager;

    private List<String> mPics = new ArrayList<String>();

    private Button mPriceBtn;

    private ProgressBar mProgressBar;

    private KiiProduct mIAPProduct;

    private boolean mIsPurchased = false;

    private KiiPayment mCurrentPayment;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_app_detail);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().setLayout(585, 556);
        mAppId = getIntent().getStringExtra(EXTRA_APP_ID);
        if (TextUtils.isEmpty(mAppId)) {
            finish();
            return;
        }

        Cursor c = getContentResolver().query(YouWill.Application.CONTENT_URI,
                null,
                YouWill.Application.APP_ID + "=(?)",
                new String[]{mAppId},
                null);
        if (c != null && c.moveToFirst()) {
            String app_info_str = c.getString(c.getColumnIndex(YouWill.Application.APP_INFO));
            try {
                mAppInfo = new JSONObject(app_info_str);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        Utils.closeSilently(c);

        if (mAppInfo == null) {
            finish();
            return;
        }

        c = getContentResolver()
                .query(YouWill.Purchased.CONTENT_URI, null, YouWill.Purchased.APP_ID + "=(?)",
                        new String[]{mAppId}, null);
        if (c != null && c.moveToFirst()) {
            String appID = c.getString(c.getColumnIndex(YouWill.Purchased.APP_ID));
            int isPurchased = c.getInt(c.getColumnIndex(YouWill.Purchased.IS_PURCHASED));
            mIsPurchased = (isPurchased == 1) && mAppId.equals(appID) && (c.getCount() == 1);
            try {
                mAppInfo.put("is_purchased", mIsPurchased);
            } catch (JSONException e) {
                LogUtils.e("Error: " + e);
            }
        }
        Utils.closeSilently(c);

        initViews();
    }

    protected void initViews() {
        ImageView closeView = (ImageView) findViewById(R.id.close);
        closeView.setOnClickListener(this);
        ImageView iconView = (ImageView) findViewById(R.id.app_detail_icon);
        String iconUrl = mAppInfo.optString("icon");
        ImageLoader.getInstance().displayImage(iconUrl, iconView, Utils.iconDisplayOptions);
        TextView tv = (TextView) findViewById(R.id.app_detail_name);
        tv.setText(mAppInfo.optString("name"));
        tv = (TextView) findViewById(R.id.app_detail_developer);
        tv.setText(mAppInfo.optString("developer_name"));
        StringBuilder info = new StringBuilder();
        tv = (TextView) findViewById(R.id.app_detail_info);
        int size = mAppInfo.optInt("apk_size");
        info.append(Utils.getFileSizeString(size)).append(" ");
        int downloadCount = mAppInfo.optInt("download_count");
        String downloadCountString = downloadCount + getString(R.string.download_count);
        info.append(downloadCountString).append(" ");
        info.append(getString(R.string.version_prompt)).append(mAppInfo
                .optString("version_name")).append(" ");
        long updateTime = mAppInfo.optLong("_modified", -1);
        if (updateTime < 0) {
            updateTime = mAppInfo.optLong("_created");
        }
        String updateTimeString = getString(R.string.update_time) + DateFormat
                .format("yyyy-MM-dd", updateTime);
        info.append(updateTimeString);
        tv.setText(info.toString());
        tv = (TextView) findViewById(R.id.app_detail_desc);
        tv.setText(mAppInfo.optString("description"));
        tv = (TextView) findViewById(R.id.app_detail_upgrade);
        tv.setText(mAppInfo.optString("what_is_new", getString(R.string.none)));
        JSONArray thumbnails = mAppInfo.optJSONArray("thumbnails");
        if (thumbnails != null && thumbnails.length() > 0) {
            for (int i = 0; i < thumbnails.length(); i++) {
                mPics.add(thumbnails.optString(i));
            }
        }
        RatingBar ratingBar = (RatingBar) findViewById(R.id.app_detail_rate);
        ratingBar.setRating((float) mAppInfo.optDouble("rating_score"));
        mPriceBtn = (Button) findViewById(R.id.app_detail_price);
        AppUtils.bindButton(this, mAppInfo, mPriceBtn);
        mProgressBar = (ProgressBar) findViewById(R.id.app_detail_progress);
        AppUtils.bindProgress(mAppId, mProgressBar, Utils.getStatus(mAppInfo));
        mPriceBtn.setOnClickListener(this);
        mRecyclerView = (RecyclerView) findViewById(R.id.pic_layout);
        mLinearLayoutManager = new LinearLayoutManager(this);
        mLinearLayoutManager.setOrientation(LinearLayoutManager.HORIZONTAL);
        mRecyclerView.setLayoutManager(mLinearLayoutManager);
        PicAdapter adapter = new PicAdapter();
        mRecyclerView.setAdapter(adapter);
        if (adapter.getItemCount() > 0) {
            mRecyclerView.setVisibility(View.VISIBLE);
            mRecyclerView.scrollToPosition(10000);
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.app_detail_price:
                double price = mAppInfo.optDouble("price");
                if ((price > 0) && !mIsPurchased) {
                    String iapID = mAppInfo.optString("iap_id");
                    checkIAP(iapID, price);
                } else {
                    refreshAppStatus();
                }
                break;
            case R.id.close:
                finish();
                break;
            default:
                break;
        }
    }

    private void refreshAppStatus() {
        AppUtils.clickPriceButton(this, mAppInfo);
        AppUtils.bindButton(this, mAppInfo, mPriceBtn);
        AppUtils.bindProgress(mAppId, mProgressBar, Utils.getStatus(mAppInfo));
        mHandler.sendEmptyMessageDelayed(MSG_UPDATE_PROGRESS, DELAY_TIME);
    }

    private void checkIAP(final String iapID, final double price) {
        Utils.showProgressDialog(this, "");
        new Thread(new Runnable() {
            @Override
            public void run() {
                mIAPProduct = KiiStore.getProductByID(iapID);
                if ((mIAPProduct != null) && (mIAPProduct.getPrice() == price)) {
                    mHandler.sendEmptyMessage(MSG_VALID_IAP_PRODUCT);
                } else {
                    mHandler.sendEmptyMessage(MSG_INVALID_IAP_PRODUCT);
                }
            }
        }).start();
    }

    private void selectPayment() {
        startActivityForResult(new Intent(this, PaymentSelectorActivity.class),
                Constants.REQ_CODE_SELECT_PAYMENT);
    }


    private void launchPayment(final PayType payType) {
        final KiiPaymentCallback paymentCallback = new KiiPaymentCallback() {
            @Override
            public void onSuccess() {
                LogUtils.d("KiiPaymentCallback.onSuccess");
                mHandler.sendEmptyMessage(MSG_IAP_SUCCESS);
            }

            @Override
            public void onError(int errorCode) {
                LogUtils.d("KiiPaymentCallback.onError");
                Message msg = mHandler.obtainMessage(MSG_IAP_ERROR,
                        KiiPayment.getErrorMessage(AppDetailActivity.this, errorCode));
                mHandler.sendMessage(msg);
            }
        };
        KiiUser.loginWithToken(new KiiUserCallBack() {
            @Override
            public void onLoginCompleted(int token, KiiUser user, Exception exception) {
                if (exception == null) {
                    KiiOrder order = new KiiOrder(mIAPProduct, user, payType);
                    mCurrentPayment = KiiPayment.getPayment(AppDetailActivity.this, order,
                            paymentCallback);
                    mCurrentPayment.pay();
                } else {
                    mHandler.sendEmptyMessage(MSG_LOGIN_ERROR);
                }
            }
        }, Settings.getToken(AppDetailActivity.this));
    }

    private static final int MSG_UPDATE_PROGRESS = 0;

    private static final int MSG_VALID_IAP_PRODUCT = 100;

    private static final int MSG_INVALID_IAP_PRODUCT = 101;

    private static final int MSG_LAUNCH_IAP = 102;

    private static final int MSG_IAP_SUCCESS = 200;

    private static final int MSG_IAP_ERROR = 201;

    private static final int MSG_LOGIN_ERROR = 202;

    private static final int DELAY_TIME = 1000;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_UPDATE_PROGRESS:
                    AppUtils.bindProgress(mAppId, mProgressBar, Utils.getStatus(mAppInfo));
                    AppUtils.bindButton(AppDetailActivity.this, mAppInfo, mPriceBtn);
                    sendEmptyMessageDelayed(MSG_UPDATE_PROGRESS, DELAY_TIME);
                    break;
                case MSG_VALID_IAP_PRODUCT:
                    Utils.dismissProgressDialog();
                    selectPayment();
                    break;
                case MSG_LAUNCH_IAP:
                    PayType payType = (PayType) msg.obj;
                    launchPayment(payType);
                    break;
                case MSG_INVALID_IAP_PRODUCT:
                    Utils.dismissProgressDialog();
                    Toast.makeText(AppDetailActivity.this, getString(R.string.invalid_iap_product),
                            Toast.LENGTH_SHORT).show();
                    break;
                case MSG_LOGIN_ERROR:
                    Toast.makeText(AppDetailActivity.this, getString(R.string.please_login),
                            Toast.LENGTH_SHORT).show();
                    break;
                case MSG_IAP_SUCCESS:
                    postIAP();
                    break;
                case MSG_IAP_ERROR:
                    String text = (String) msg.obj;
                    Toast.makeText(AppDetailActivity.this, text, Toast.LENGTH_SHORT).show();
                    break;
                default:
                    break;
            }
        }
    };

    private void postIAP() {
        mIsPurchased = true;
        DataUtils.appendPurchasedApp(this, mAppId);
        try {
            mAppInfo.put("is_purchased", mIsPurchased);
        } catch (JSONException e) {
            LogUtils.e("Error: " + e);
        }
        refreshAppStatus();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case Constants.REQ_CODE_SELECT_PAYMENT:
                if (resultCode == Activity.RESULT_OK) {
                    PayType payType = null;
                    try {
                        payType = PayType
                                .valueOf(data.getStringExtra(Constants.INTENT_EXTRA_PAY_TYPE));
                    } catch (Exception e) {
                        payType = PayType.alipay;
                        LogUtils.e(e);
                    }
                    Settings.setLastUsedPayType(this, payType);
                    Message msg = mHandler.obtainMessage(MSG_LAUNCH_IAP, payType);
                    mHandler.sendMessage(msg);
                }
                break;
            default:
                super.onActivityResult(requestCode, resultCode, data);
        }

        if (mCurrentPayment != null) {
            mCurrentPayment.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    protected void onDestroy() {
        try {
            mHandler.removeMessages(MSG_UPDATE_PROGRESS);
            mHandler = null;
        } catch (Exception e) {
            LogUtils.e(e);
        }
        super.onDestroy();
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {

        private ImageView mImageView;

        public ViewHolder(View itemView) {
            super(itemView);
            mImageView = (ImageView) itemView.findViewById(R.id.image);
        }

    }

    private class PicAdapter extends RecyclerView.Adapter<ViewHolder> {

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int i) {
            View v = getLayoutInflater().inflate(R.layout.app_detail_pic_item_portrait, viewGroup,
                    false);
            return new ViewHolder(v);
        }

        @Override
        public void onBindViewHolder(ViewHolder viewHolder, int i) {
            if (mPics.size() == 0) {
                return;
            }
            final String url = mPics.get(i % mPics.size());
            ImageLoader.getInstance()
                    .displayImage(url, viewHolder.mImageView, Utils.detailDisplayOptions);
            viewHolder.mImageView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    Intent intent = new Intent(AppDetailActivity.this, ViewImageActivity.class);
                    intent.putExtra(Constants.INTENT_EXTRA_URL, url);
                    startActivity(intent);
                }
            });
        }

        @Override
        public int getItemCount() {
            return Integer.MAX_VALUE;
        }
    }

}
