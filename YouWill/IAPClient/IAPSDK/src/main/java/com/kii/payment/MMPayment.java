package com.kii.payment;

import android.app.Activity;
import android.app.ProgressDialog;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import com.kii.payment.impl.PrefUtil;
import mm.purchasesdk.OnPurchaseListener;
import mm.purchasesdk.Purchase;
import mm.purchasesdk.PurchaseCode;
import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;

/**
 * Created by liangyx on 11/27/14.
 */
class MMPayment extends KiiPayment {

    private static final String TAG = MMPayment.class.getName();
    Purchase purchase;

    private ProgressDialog progressDialog;

    public MMPayment(Activity activity, KiiOrder order, KiiPaymentCallback callback) {
        super(activity, order, callback);
        init();
    }

    private void init() {
        Utils.setContextRef(context);
    }

    @Override
    public void pay() {
        initMMPayment();
        getTransactionFromServer();
    }

    private void getTransactionFromServer() {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(context);
        }
        progressDialog.setCancelable(false);
        progressDialog.setIndeterminate(true);
        progressDialog.setTitle(Utils.getStringResId("kii_payment_fetching_transaction_from_cloud"));
        progressDialog.show();
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    HttpClient client = new DefaultHttpClient();
                    String url = String.format(Constants.GET_PARAM_URL, order.getProductId());
                    HttpPost request = new HttpPost(url);

                    JSONObject jsonObj = new JSONObject();
                    jsonObj.put("author_id", YouWillIAPSDK.getAuthorID());
                    jsonObj.put("app_id", YouWillIAPSDK.getYouWillAppID());
                    jsonObj.put("pay_type", order.payType);
                    jsonObj.put("price", Double.toString(order.price));
                    jsonObj.put("is_sandbox", "0");
                    jsonObj.put("user_id", order.userId);
                    Utils.log(TAG, "post parameter is " + jsonObj.toString());
                    request.setEntity(new StringEntity(jsonObj.toString()));

                    HttpResponse response = client.execute(request);
                    String result = EntityUtils.toString(response.getEntity());

                    Utils.log(TAG, "result is " + result);

                    JSONObject object = new JSONObject(result);
                    int errorCode = object.getInt("errorCode");
                    if (errorCode == 1001) {
                        handler.sendEmptyMessage(MSG_PRODUCT_ALREADY_PURCHASED);
                    } else {
                        JSONObject mmObj = object.optJSONObject("mm");
                        if (mmObj == null) {
                            Message msg = handler.obtainMessage(MSG_MM_PAY_FAILED);
                            msg.arg1 = KiiPaymentResultCode.CREATE_ORDER_ERROR;
                            handler.sendMessage(msg);
                        } else {
                            Message msg = handler.obtainMessage(MSG_GET_PARAMS);
                            msg.obj = mmObj;
                            handler.sendMessage(msg);
                        }
                    }

                } catch (Exception e) {
                    e.printStackTrace();
                    Message msg = handler.obtainMessage(MSG_MM_PAY_FAILED);
                    msg.arg1 = KiiPaymentResultCode.ERROR_NETWORK_EXCEPTION;
                    handler.sendMessage(msg);
                }
            }
        }).start();
    }

    private static final int MSG_GET_PARAMS = 0;
    private static final int MSG_FINISH_TRANSACTION = 1;
    private static final int MSG_MM_CLIENT_FINISHED = 2;
    private static final int MSG_MM_PAY_FAILED = 5;
    private static final int MSG_MM_CONFIRM_CLOUD_FAILED = 6;

    private static final int MSG_PRODUCT_ALREADY_PURCHASED = 1001;

    private Handler handler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_GET_PARAMS:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    payWithMM((JSONObject) msg.obj);
                    break;
                case MSG_FINISH_TRANSACTION:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onSuccess();
                    break;
                case MSG_MM_CLIENT_FINISHED:
                    confirmWithCloud((JSONObject) msg.obj);
                    break;

                case MSG_MM_CONFIRM_CLOUD_FAILED:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onError(KiiPaymentResultCode.ALIPAY_VERIFY_FAILED);
                    break;
                case MSG_MM_PAY_FAILED:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    int code = KiiPaymentResultCode.ERROR_UNKNOWN_ERROR;
                    if (msg.arg1 != 0) {
                        code = msg.arg1;
                    }
                    callback.onError(code);
                    break;
                case MSG_PRODUCT_ALREADY_PURCHASED:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onError(KiiPaymentResultCode.PRODUCT_ALREADY_BOUGHT);
                    break;

            }
        }
    };

    private void payWithMM(JSONObject mmObj) {
        String payCode = mmObj.optString("pay_code");
        String transactionID = mmObj.optString("transaction_id");
        MMIapListener iapListener = new MMIapListener(mmObj);
        try {
            purchase.order(context, payCode, 1, transactionID, false, iapListener);
        } catch (Exception e) {
            Message msg = handler.obtainMessage(MSG_MM_PAY_FAILED, KiiPaymentResultCode.ERROR_MM_INVALID_PAY_CODE, 0);
            handler.sendMessage(msg);
        }
    }

    private void confirmWithCloud(JSONObject mmObj) {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(context);
        }
        progressDialog.setCancelable(false);
        progressDialog.setIndeterminate(true);
        progressDialog.setTitle(Utils.getStringResId("kii_payment_confirm_payment_from_cloud"));
        progressDialog.show();

        final String transactionID = mmObj.optString("transaction_id");
        PrefUtil.cacheClientPaymentStatus(context, order.getProductId(), transactionID);
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    URL url = new URL(String.format(Constants.QUERY_ORDER_URL, transactionID));
                    URLConnection conn = url.openConnection();
                    BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
                    String result = reader.readLine();
                    Log.e("test", "result: " + result);
                    JSONObject jsonObject = new JSONObject(result);
                    String payStatus = jsonObject.optString("payStatus");
                    if (STATUS_COMPLETED.equals(payStatus)) {
                        Log.e("test", "Confirmed with Cloud.");
                        //TODO: mTransactionInfo = result;
                        clearCachedResult();
                        handler.sendEmptyMessage(MSG_FINISH_TRANSACTION);
                    } else {
                        Log.e("test", "Cloud status: " + payStatus);
                        handler.sendEmptyMessage(MSG_MM_CONFIRM_CLOUD_FAILED);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    Log.e("test", "Error while querying order.");
                    handler.sendEmptyMessage(MSG_MM_CONFIRM_CLOUD_FAILED);
                }

            }
        }).start();
    }


    private void initMMPayment() {
        purchase = Purchase.getInstance();
        purchase.setAppInfo("300008405924", "295D1FF50C521526");
        MMIapListener iapListener = new MMIapListener(null);
        purchase.init(context, iapListener);
    }

    class MMIapListener implements OnPurchaseListener {
        JSONObject mmObj;

        MMIapListener(JSONObject mmObj) {
            this.mmObj = mmObj;
        }

        @Override
        public void onBeforeApply() {
            //
        }

        @Override
        public void onAfterApply() {
            //
        }

        @Override
        public void onBeforeDownload() {
            //
        }

        @Override
        public void onAfterDownload() {
            //
        }

        @Override
        public void onQueryFinish(int i, HashMap hashMap) {
            //
        }

        @Override
        public void onBillingFinish(int code, HashMap data) {
            if (code == PurchaseCode.ORDER_OK || (code == PurchaseCode.AUTH_OK)) {
                if (data != null) {
                    String paycode = (String) data
                            .get(OnPurchaseListener.PAYCODE);
                    if (!TextUtils.isEmpty(paycode)) {
                        Message msg = handler.obtainMessage(MSG_MM_CLIENT_FINISHED, mmObj);
                        handler.sendMessage(msg);
                    }
                }
            } else {
                //String result = "Payment faild: " + Purchase.getReason(code);
                handler.sendEmptyMessage(MSG_MM_PAY_FAILED);
            }

        }

        @Override
        public void onUnsubscribeFinish(int i) {
            //
        }

        @Override
        public void onInitFinish(int code) {
            if (code == PurchaseCode.INIT_OK) {
                Utils.log(TAG, "MM init OK");
            } else {
                Utils.log(TAG, "MM init failed.");
            }
        }
    }

}
