package com.youwill.store.view;

import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.os.Handler;
import android.os.Message;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;
import com.kii.payment.*;
import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.net.DownloadAgent;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.*;
import com.youwill.store.utils.Utils;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by Evan on 14/10/19.
 */
public class AppGridAdapter extends CursorAdapter implements View.OnClickListener {

    protected Context mContext;

    public AppGridAdapter(Context context, Cursor c, int flags) {
        super(context, c, 0);
        mContext = context;
    }

    @Override
    public View newView(Context context, Cursor cursor, ViewGroup viewGroup) {
        return LayoutInflater.from(context).inflate(R.layout.app_grid_item, viewGroup, false);
    }

    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        String app_info_str = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_INFO));
        int isPurchased = cursor.getInt(cursor.getColumnIndex(YouWill.Purchased.IS_PURCHASED));

        JSONObject appInfo = null;
        try {
            appInfo = new JSONObject(app_info_str);
            appInfo.put("is_purchased", isPurchased == 1);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        if (appInfo == null) {
            return;
        }
        TextView tv = (TextView) view.findViewById(R.id.app_grid_pos);
        tv.setText(String.valueOf(cursor.getPosition() + 1));
        ImageView iconView = (ImageView) view.findViewById(R.id.app_grid_icon);
        String iconUrl = appInfo.optString("icon");
        ImageLoader.getInstance().displayImage(iconUrl, iconView, Utils.iconDisplayOptions);
        Button price_btn = (Button) view.findViewById(R.id.app_grid_price);
        price_btn.setTag(appInfo);
        AppUtils.bindButton(context, appInfo, price_btn);

        price_btn.setOnClickListener(this);
        price_btn.setFocusable(false);
        price_btn.setFocusableInTouchMode(false);
        tv = (TextView) view.findViewById(R.id.app_grid_name);
        tv.setText(appInfo.optString("name"));
        RatingBar bar = (RatingBar) view.findViewById(R.id.app_grid_rate);
        bar.setRating((float) appInfo.optDouble("rating_score"));
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.app_grid_price:
                JSONObject appInfo = (JSONObject) v.getTag();
                double price = appInfo.optDouble("price");
                boolean mIsPurchased = appInfo.optBoolean("is_purchased", false);
                if ((price > 0) && !mIsPurchased) {
                    String iapID = appInfo.optString("iap_id");
                    checkIAP(iapID, price);
                } else {
                    refreshAppStatus(appInfo, v);
                }

                break;
            default:
                break;
        }
    }

    private void checkIAP(final String iapID, final double price) {
        Utils.showProgressDialog((Activity) mContext, "");
        new Thread(new Runnable() {
            @Override
            public void run() {
                KiiProduct product = KiiStore.getProductByID(iapID);
                if ((product != null) && (product.getPrice() == price)) {
                    Message msg = mHandler.obtainMessage(MSG_VALID_IAP_PRODUCT, product);
                    mHandler.sendMessage(msg);
                } else {
                    mHandler.sendEmptyMessage(MSG_INVALID_IAP_PRODUCT);
                }
            }
        }).start();
    }

    private void refreshAppStatus(JSONObject appInfo, View v) {
        AppUtils.clickPriceButton(mContext, appInfo);
        AppUtils.bindButton(mContext, appInfo, (Button) v);
    }

    private void launchPayment(final KiiProduct product) {
        final KiiPaymentCallback paymentCallback = new KiiPaymentCallback() {
            @Override
            public void onSuccess() {
                LogUtils.d("KiiPaymentCallback.onSuccess");
                Message msg = mHandler.obtainMessage(MSG_IAP_SUCCESS, product.getFieldByName("app"));
                mHandler.sendMessage(msg);
            }

            @Override
            public void onError(int errorCode) {
                LogUtils.d("KiiPaymentCallback.onError");
                Message msg = mHandler.obtainMessage(MSG_IAP_ERROR,
                        KiiPayment.getErrorMessage(mContext, errorCode));
                mHandler.sendMessage(msg);
            }
        };
        KiiUser.loginWithToken(new KiiUserCallBack() {
            @Override
            public void onLoginCompleted(int token, KiiUser user, Exception exception) {
                if (exception == null) {
                    KiiOrder order = new KiiOrder(product, user);
                    KiiPayment currentPayment = new KiiPayment((Activity) mContext, order,
                            paymentCallback);
                    currentPayment.pay();
                } else {
                    mHandler.sendEmptyMessage(MSG_LOGIN_ERROR);
                }
            }
        }, Settings.getToken(mContext));
    }

    private void postIAP(String appID) {
        DataUtils.appendPurchasedApp(mContext, appID);
        mContext.getContentResolver().notifyChange(YouWill.Application.CONTENT_URI, null);
        DownloadAgent.getInstance().beginDownload(appID);

    }

    private static final int MSG_VALID_IAP_PRODUCT = 100;

    private static final int MSG_INVALID_IAP_PRODUCT = 101;

    private static final int MSG_IAP_SUCCESS = 200;

    private static final int MSG_IAP_ERROR = 201;

    private static final int MSG_LOGIN_ERROR = 202;

    private static final int DELAY_TIME = 1000;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {

                case MSG_VALID_IAP_PRODUCT:
                    Utils.dismissProgressDialog();
                    KiiProduct product = (KiiProduct) msg.obj;
                    launchPayment(product);
                    break;
                case MSG_INVALID_IAP_PRODUCT:
                    Utils.dismissProgressDialog();
                    Toast.makeText(mContext, mContext.getString(R.string.invalid_iap_product),
                            Toast.LENGTH_SHORT).show();
                    break;
                case MSG_LOGIN_ERROR:
                    Toast.makeText(mContext, mContext.getString(R.string.please_login),
                            Toast.LENGTH_SHORT).show();
                    break;
                case MSG_IAP_SUCCESS:
                    String appID = (String) msg.obj;
                    postIAP(appID);
                    break;
                case MSG_IAP_ERROR:
                    String text = (String) msg.obj;
                    Toast.makeText(mContext, text, Toast.LENGTH_SHORT).show();
                    break;
                default:
                    break;
            }
        }
    };

}
