package com.kii.payment;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Fragment;
import android.app.ProgressDialog;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;
import android.util.SparseIntArray;
import com.alipay.android.app.sdk.AliPay;
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
import java.lang.reflect.Field;
import java.net.URL;
import java.net.URLConnection;

/**
 * Created by tian on 3/9/14.
 * Modified by Richard
 */
@SuppressLint("NewApi")
public class KiiPayment {

    private static final String TAG = KiiPayment.class.getName();

    private Activity context;

    private KiiOrder order;

    private KiiPaymentCallback callback;

    private String mTransactionInfo;

    private static String STATUS_COMPLETED = "completed";

    /**
     * @param activity
     * @param order
     * @param callback
     */
    public KiiPayment(Activity activity, KiiOrder order, KiiPaymentCallback callback) {
        this.context = activity;
        this.order = order;
        this.callback = callback;
        init();
    }

    /**
     * @param fragment
     * @param order
     * @param callback
     */
    public KiiPayment(Fragment fragment, KiiOrder order, KiiPaymentCallback callback) {
        this.context = fragment.getActivity();
        this.order = order;
        this.callback = callback;

        init();
    }

    /**
     * @param fragment
     * @param order
     * @param callback
     */
    public KiiPayment(android.support.v4.app.Fragment fragment, KiiOrder order,
                      KiiPaymentCallback callback) {
        this.context = fragment.getActivity();
        this.order = order;
        this.callback = callback;

        init();
    }

    /**
     * @param context
     * @param errorCode
     * @return
     */
    public static String getErrorMessage(Context context, int errorCode) {
        try {
            Class<?> c = Class.forName(context.getApplicationContext().getPackageName()
                    + ".R$string");
            if (c != null) {
                Field field = c.getField(sErrorMap.get(errorCode));
                Object property = field.get(c);
                return context.getString((Integer) property);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "未知错误";
    }

    private void init() {
        Utils.setContextRef(context);
    }

    /**
     *
     */
    public void pay() {
        //TODO: check client and cloud status.
        //clearCachedResult();
        String orderID = PrefUtil.getCachedTransactionID(context, order.getProductId());
        if (!TextUtils.isEmpty(orderID)) {
            confirmWithCloud(orderID);
            return;
        }
        order.payType = KiiOrder.ALIPAY;
        getTransactionFromServer();
    }

    public String getTransactionInfo() {
        return mTransactionInfo;
    }

    public void clearCachedResult() {
        PrefUtil.clearCachedResult(context, order.getProductId());
    }

    public int getRetryNum() {
        return PrefUtil.getRetryNum(context, order.getProductId());
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
                            String.format(Constants.PLATFORM_CALLBACK_URL,
                                    YouWillIAPSDK.getAuthorID(), YouWillIAPSDK.getYouWillAppID()));
                    jsonObj.put("payType", order.payType);
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
                    callback.onError(KiiPaymentResultCode.ERROR_NETWORK_EXCEPTION);
                }
            }
        }).start();
    }

    private static final int MSG_GET_PARAMS = 0;

    private static final int MSG_FINISH_TRANSACTION = 1;

    private static final int MSG_ALIPAY_CLIENT_FINISHED = 2;

    private static final int MSG_PAY_FAILED = 5;

    private static final int MSG_ALIPAY_CONFIRM_CLOUD_FAILED = 6;

    private static final int  MSG_PRODUCT_ALREADY_PURCHASED = 1001;

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

    private static final SparseArray<String> sErrorMap = new SparseArray<String>();

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

        if (sErrorMap.size() == 0) {
            sErrorMap.put(KiiPaymentResultCode.SUCCESS, "kii_payment_success");
            sErrorMap.put(KiiPaymentResultCode.ERROR_SYSTEM_EXCEPTION,
                    "kii_payment_error_remote_server_exception");
            sErrorMap.put(KiiPaymentResultCode.ERROR_DATA_FORMAT, "kii_payment_error_data_format");
            sErrorMap.put(KiiPaymentResultCode.ERROR_ACCOUNT_FROZEN,
                    "kii_payment_error_account_frozen");
            sErrorMap.put(KiiPaymentResultCode.ERROR_ACCOUNT_NOT_BIND,
                    "kii_payment_error_account_not_bind");
            sErrorMap.put(KiiPaymentResultCode.ERROR_BIND_FAILED, "kii_payment_error_bind_failed");
            sErrorMap.put(KiiPaymentResultCode.ERROR_PAY_FAILED, "kii_payment_error_pay_failed");
            sErrorMap.put(KiiPaymentResultCode.ERROR_ACCOUNT_NEED_REBIND,
                    "kii_payment_error_need_rebind");
            sErrorMap.put(KiiPaymentResultCode.ERROR_SERVER_UPGRADING,
                    "kii_payment_error_server_upgrading");
            sErrorMap.put(KiiPaymentResultCode.ERROR_PAYMENT_CANCELLED,
                    "kii_payment_error_payment_cancelled");
            sErrorMap.put(KiiPaymentResultCode.ERROR_NETWORK_EXCEPTION,
                    "kii_payment_error_network_exception");
            sErrorMap.put(KiiPaymentResultCode.ERROR_INVALID_ORDER,
                    "kii_payment_error_invalid_order");
            sErrorMap.put(KiiPaymentResultCode.ERROR_UNKNOWN_ERROR, "kii_payment_error_unknown");
            sErrorMap.put(KiiPaymentResultCode.METHOD_ERROR, "kii_payment_error_network_exception");
            sErrorMap.put(KiiPaymentResultCode.PARAMETER_ERROR, "kii_payment_error_network_exception");
            sErrorMap.put(KiiPaymentResultCode.PARAMETER_ERROR2, "kii_payment_error_network_exception");
            sErrorMap.put(KiiPaymentResultCode.PARAMETER_ERROR3, "kii_payment_error_network_exception");
            sErrorMap.put(KiiPaymentResultCode.METHOD_ERROR2, "kii_payment_error_network_exception");
            sErrorMap.put(KiiPaymentResultCode.PARAMETER_ERROR4, "kii_payment_error_network_exception");
            sErrorMap.put(KiiPaymentResultCode.PRODUCT_NOT_FOUND, "kii_payment_error_product_not_found");
            sErrorMap.put(KiiPaymentResultCode.PRODUCT_ALREADY_BOUGHT, "kii_payment_error_product_already_bought");

            sErrorMap.put(KiiPaymentResultCode.ALIPAY_NOT_SUPPORTED, "kii_payment_error_alipay_not_support");
            sErrorMap.put(KiiPaymentResultCode.PAYPAL_NOT_SUPPORTED, "kii_payment_error_paypal_not_support");
            sErrorMap.put(KiiPaymentResultCode.UNKNOWN_PAYMENT_METHOD, "kii_payment_error_unknown_payment");
            sErrorMap.put(KiiPaymentResultCode.CREATE_ORDER_ERROR, "kii_payment_error_create_order_failed");
            sErrorMap.put(KiiPaymentResultCode.ORDER_DOES_NOT_EXIST, "kii_payment_error_order_not_exist");
            sErrorMap.put(KiiPaymentResultCode.UPDATE_ORDER_FAILED, "kii_payment_error_update_order_failed");
            sErrorMap.put(KiiPaymentResultCode.CREATE_RECEIPT_FAILED, "kii_payment_error_create_receipt_failed");
            sErrorMap.put(KiiPaymentResultCode.INVALID_PAYPAL_PARAMETER, "kii_payment_error_paypal_parameters");
            sErrorMap.put(KiiPaymentResultCode.INVALID_PAYPAL_PARAMETER2, "kii_payment_error_paypal_parameters");
            sErrorMap.put(KiiPaymentResultCode.PAYPAL_VERIFY_FAILED, "kii_payment_error_verify_paypal_failed");
            sErrorMap.put(KiiPaymentResultCode.ALIPAY_VERIFY_FAILED, "kii_payment_error_verify_alipay_failed");
        }
    }

}
