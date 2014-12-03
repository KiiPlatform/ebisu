package com.kii.payment;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Fragment;
import android.app.ProgressDialog;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseIntArray;
import com.alipay.android.app.sdk.AliPay;
import com.kii.payment.impl.BaseHelper;
import com.kii.payment.impl.PrefUtil;
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

/**
 * Created by tian on 3/9/14.
 * Modified by Richard
 */
@SuppressLint("NewApi")
class AliPayPayment extends KiiPayment {

    private static final String TAG = AliPayPayment.class.getName();

    private String mTransactionInfo;

    /**
     * Constructs an instance of KiiPayment with the specified order.
     *
     * @param activity - the host activity
     * @param order    - order to pay
     * @param callback - callback listener
     */
    public AliPayPayment(Activity activity, KiiOrder order, KiiPaymentCallback callback) {
        super(activity, order, callback);
        init();
    }

    /**
     * Constructs an instance of KiiPayment with the specified order.
     *
     * @param fragment - the host fragment
     * @param order    - the order to pay
     * @param callback - callback listener
     */
    public AliPayPayment(Fragment fragment, KiiOrder order, KiiPaymentCallback callback) {
        super(fragment.getActivity(), order, callback);
        init();
    }

    /**
     * Constructs an instance of KiiPayment with the specified order.
     *
     * @param fragment - the host fragment
     * @param order    - the order to pay
     * @param callback - callback listener
     */
    public AliPayPayment(android.support.v4.app.Fragment fragment, KiiOrder order,
                         KiiPaymentCallback callback) {
        super(fragment.getActivity(), order, callback);
        init();
    }

    private void init() {
        Utils.setContextRef(context);
    }

    @Override
    public void pay() {
        //clearCachedResult();
        String orderID = PrefUtil.getCachedTransactionID(context, order.getProductId());
        if (!TextUtils.isEmpty(orderID)) {
            confirmWithCloud(orderID);
            return;
        }
        getTransactionFromServer();
    }

    /**
     * Get the transaction information after the payment is successful.
     *
     * @return - the transaction information in json format.
     */
    public String getTransactionInfo() {
        return mTransactionInfo;
    }


    private ProgressDialog progressDialog;

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
                    jsonObj.put("notify_url",
                            String.format(Constants.PLATFORM_CALLBACK_URL_ALIPAY,
                                    YouWillIAPSDK.getAuthorID(), YouWillIAPSDK.getYouWillAppID()));
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
                        String orderInfo = object.getString("url");
                        Message msg = handler.obtainMessage(MSG_GET_PARAMS);
                        msg.obj = orderInfo;
                        handler.sendMessage(msg);
                    }

                } catch (Exception e) {
                    e.printStackTrace();
                    handler.sendEmptyMessage(MSG_PAY_FAILED);
                }
            }
        }).start();
    }

    private static final int MSG_GET_PARAMS = 0;

    private static final int MSG_FINISH_TRANSACTION = 1;

    private static final int MSG_ALIPAY_CLIENT_FINISHED = 2;

    private static final int MSG_PAY_FAILED = 5;

    private static final int MSG_ALIPAY_CONFIRM_CLOUD_FAILED = 6;

    private static final int MSG_PRODUCT_ALREADY_PURCHASED = 1001;

    private Handler handler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_GET_PARAMS:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    payWithAlipay((String) msg.obj);
                    break;
                case MSG_FINISH_TRANSACTION:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onSuccess();
                    break;
                case MSG_ALIPAY_CLIENT_FINISHED:
                    handleAlipayFinishStatus(msg);
                    break;

                case MSG_ALIPAY_CONFIRM_CLOUD_FAILED:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onError(KiiPaymentResultCode.ALIPAY_VERIFY_FAILED);
                    break;
                case MSG_PAY_FAILED:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    callback.onError(KiiPaymentResultCode.ERROR_UNKNOWN_ERROR);
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

    private void payWithAlipay(final String orderInfo) {
        new Thread() {
            public void run() {
                Utils.log(TAG, "orderInfo is " + orderInfo);
                AliPay alipay = new AliPay(context, handler);
                String result = alipay.pay(orderInfo);
                Message msg = new Message();
                msg.what = MSG_ALIPAY_CLIENT_FINISHED;
                msg.obj = result;
                handler.sendMessage(msg);
            }
        }.start();
    }

    private void handleAlipayFinishStatus(Message msg) {
        String strRet = (String) msg.obj;
        Utils.log(TAG, "handleAlipayFinishedStatus, strRet is " + strRet);
        try {
            JSONObject jsonObject = BaseHelper.string2JSON(strRet, ";");
            String tradeStatus = jsonObject.getString("resultStatus");
            tradeStatus = tradeStatus.substring(1, tradeStatus.length() - 1);
            if (tradeStatus.equals("9000")) {
                String result = jsonObject.getString("result");
                result = result.substring(1, result.length() - 1);
                jsonObject = BaseHelper.string2JSON(result, "&");
                String transactionID = jsonObject.getString("out_trade_no").replace("\"", "");
                confirmWithCloud(transactionID);
            } else {
                if (callback != null) {
                    int alipayErrorCode = 0;
                    try {
                        alipayErrorCode = Integer.parseInt(tradeStatus);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                    if (sAlipayMap.get(alipayErrorCode) != 0) {
                        callback.onError(sAlipayMap.get(alipayErrorCode));
                    } else {
                        callback.onError(KiiPaymentResultCode.ERROR_UNKNOWN_ERROR);
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
            callback.onError(KiiPaymentResultCode.ERROR_UNKNOWN_ERROR);
        }

    }

    private void confirmWithCloud(final String orderID) {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(context);
        }
        progressDialog.setCancelable(false);
        progressDialog.setIndeterminate(true);
        progressDialog.setTitle(Utils.getStringResId("kii_payment_confirm_payment_from_cloud"));
        progressDialog.show();

        PrefUtil.cacheClientPaymentStatus(context, order.getProductId(), orderID);
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    URL url = new URL(String.format(Constants.QUERY_ORDER_URL, orderID));
                    URLConnection conn = url.openConnection();
                    BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
                    String result = reader.readLine();
                    Log.e("test", "result: " + result);
                    JSONObject jsonObject = new JSONObject(result);
                    String payStatus = jsonObject.optString("payStatus");
                    if (STATUS_COMPLETED.equals(payStatus)) {
                        Log.e("test", "Confirmed with Cloud.");
                        mTransactionInfo = result;
                        clearCachedResult();
                        handler.sendEmptyMessage(MSG_FINISH_TRANSACTION);
                    } else {
                        Log.e("test", "Cloud status: " + payStatus);
                        handler.sendEmptyMessage(MSG_ALIPAY_CONFIRM_CLOUD_FAILED);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    Log.e("test", "Error while querying order.");
                    handler.sendEmptyMessage(MSG_ALIPAY_CONFIRM_CLOUD_FAILED);
                }

            }
        }).start();
    }


    private static final SparseIntArray sAlipayMap = new SparseIntArray();

    static {
        if (sAlipayMap.size() == 0) {
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.SUCCESS, KiiPaymentResultCode.SUCCESS);
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.SYSTEM_EXCEPTION,
                    KiiPaymentResultCode.ERROR_SYSTEM_EXCEPTION);
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.ERROR_DATA_FORMAT,
                    KiiPaymentResultCode.ERROR_DATA_FORMAT);
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.ACCOUNT_FROZEN,
                    KiiPaymentResultCode.ERROR_ACCOUNT_FROZEN);
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.ACCOUNT_NOT_BOUND,
                    KiiPaymentResultCode.ERROR_ACCOUNT_NOT_BIND);
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.ACCOUNT_BIND_FAILED,
                    KiiPaymentResultCode.ERROR_BIND_FAILED);
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.PAY_FAILED,
                    KiiPaymentResultCode.ERROR_PAY_FAILED);
            sAlipayMap
                    .put(Constants.ALIPAY_RESULT_CODE.ACCOUNT_NEED_REBIND,
                            KiiPaymentResultCode.ERROR_ACCOUNT_NEED_REBIND);
            sAlipayMap.put(Constants.ALIPAY_RESULT_CODE.SERVER_UPGRADING,
                    KiiPaymentResultCode.ERROR_SERVER_UPGRADING);
            sAlipayMap
                    .put(Constants.ALIPAY_RESULT_CODE.PAYMENT_CANCELLED,
                            KiiPaymentResultCode.ERROR_PAYMENT_CANCELLED);
            sAlipayMap
                    .put(Constants.ALIPAY_RESULT_CODE.NETWORK_EXCEPTION,
                            KiiPaymentResultCode.ERROR_NETWORK_EXCEPTION);
        }

    }

}
