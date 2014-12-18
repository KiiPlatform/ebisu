package com.kii.payment;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;

import com.kii.payment.impl.PrefUtil;
import com.unionpay.UPPayAssistEx;

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

import mm.purchasesdk.core.utils.LogUtil;

/**
 * Created by liangyx on 12/10/14.
 */
public class UnionPayPayment extends KiiPayment {

    private static final String TAG = UnionPayPayment.class.getName();

    private ProgressDialog progressDialog;

    private JSONObject currentUPObj;

    UnionPayPayment(Activity activity, KiiOrder order, KiiPaymentCallback callback) {
        super(activity, order, callback);
    }

    @Override
    public void pay() {
        String orderID = PrefUtil.getCachedTransactionID(context, order.getProductId());
        if (!TextUtils.isEmpty(orderID)) {
            confirmWithCloud();
            return;
        }
        getTransactionFromServer();
    }

    private void getTransactionFromServer() {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(context);
        }
        progressDialog.setCancelable(false);
        progressDialog.setIndeterminate(true);
        progressDialog.setTitle(Utils.getStringResId("kii_payment_fetching_transaction_from_cloud"));
        try {
            progressDialog.show();
        } catch (Exception e) {
            Utils.log(TAG, "Error: " + e);
        }
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

                    if (errorCode == 0) {
                        JSONObject upObj = object.optJSONObject("up");
                        if (upObj == null) {
                            Message msg = handler.obtainMessage(MSG_UNION_PAY_FAILED);
                            msg.arg1 = KiiPaymentResultCode.CREATE_ORDER_ERROR;
                            handler.sendMessage(msg);
                        } else {
                            Message msg = handler.obtainMessage(MSG_GET_PARAMS);
                            msg.obj = upObj;
                            handler.sendMessage(msg);
                        }
                    } else if (errorCode == 1001) {
                        handler.sendEmptyMessage(MSG_PRODUCT_ALREADY_PURCHASED);
                    } else if (errorCode == 1003) {
                        Message msg = handler.obtainMessage(MSG_UNION_PAY_FAILED);
                        msg.arg1 = KiiPaymentResultCode.CREATE_ORDER_ERROR;
                        handler.sendMessage(msg);
                    }

                } catch (Exception e) {
                    e.printStackTrace();
                    Message msg = handler.obtainMessage(MSG_UNION_PAY_FAILED);
                    msg.arg1 = KiiPaymentResultCode.ERROR_NETWORK_EXCEPTION;
                    handler.sendMessage(msg);
                }
            }
        }).start();
    }

    private void confirmWithCloud() {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(context);
        }
        progressDialog.setCancelable(false);
        progressDialog.setIndeterminate(true);
        progressDialog.setTitle(Utils.getStringResId("kii_payment_confirm_payment_from_cloud"));
        progressDialog.show();

        final String transactionID = currentUPObj.optString("transaction_id");
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
                        handler.sendEmptyMessage(MSG_UNION_CONFIRM_CLOUD_FAILED);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    Log.e("test", "Error while querying order.");
                    handler.sendEmptyMessage(MSG_UNION_CONFIRM_CLOUD_FAILED);
                }

            }
        }).start();
    }

    private static final int MSG_GET_PARAMS = 0;
    private static final int MSG_FINISH_TRANSACTION = 1;
    private static final int MSG_UNION_CLIENT_FINISHED = 2;
    private static final int MSG_UNION_PAY_FAILED = 5;
    private static final int MSG_UNION_CONFIRM_CLOUD_FAILED = 6;
    private static final int MSG_UNION_PLUGIN_INVALID = 7;
    private static final int MSG_UNION_PAY_CANCELLED = 8;

    private static final int MSG_PRODUCT_ALREADY_PURCHASED = 1001;

    private Handler handler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_GET_PARAMS:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    currentUPObj = (JSONObject) msg.obj;
                    payWithUnionPay();
                    break;
                case MSG_FINISH_TRANSACTION:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onSuccess();
                    break;
                case MSG_UNION_CLIENT_FINISHED:
                    confirmWithCloud();
                    break;

                case MSG_UNION_CONFIRM_CLOUD_FAILED:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onError(KiiPaymentResultCode.PAYMENT_VERIFY_FAILED);
                    break;
                case MSG_UNION_PAY_FAILED:
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
                case MSG_UNION_PLUGIN_INVALID:
                    callback.onError(KiiPaymentResultCode.ERROR_UNION_PLUGIN_INVALID);
                    break;
                case MSG_UNION_PAY_CANCELLED:
                    callback.onError(KiiPaymentResultCode.ERROR_PAYMENT_CANCELLED);
                    break;
            }
        }
    };

    private void payWithUnionPay() {
        int ret = UPPayAssistEx.startPay(context, null, null, currentUPObj.optString("tn"), "00");
        if (ret != UPPayAssistEx.PLUGIN_VALID) {
            //handler.sendEmptyMessage(MSG_UNION_PLUGIN_INVALID);
            if (!UPPayAssistEx.installUPPayPlugin(context)) {
                handler.sendEmptyMessage(MSG_UNION_PLUGIN_INVALID);
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if ((data == null) || (requestCode != UNION_PAY_REQUEST_CODE)) {
            return;
        }

        String str = data.getExtras().getString("pay_result");
        if (str.equalsIgnoreCase("success")) {
            handler.sendEmptyMessage(MSG_UNION_CLIENT_FINISHED);
        } else if (str.equalsIgnoreCase("fail")) {
            handler.sendEmptyMessage(MSG_UNION_PAY_FAILED);
        } else if (str.equalsIgnoreCase("cancel")) {
            handler.sendEmptyMessage(MSG_UNION_PAY_CANCELLED);
        }
    }
}
