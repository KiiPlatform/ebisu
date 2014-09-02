
package com.kii.payment;

import java.lang.reflect.Field;
import java.math.BigDecimal;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Currency;
import java.util.List;
import java.util.Locale;

import org.apache.http.HttpResponse;
import org.apache.http.NameValuePair;
import org.apache.http.client.HttpClient;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.message.BasicNameValuePair;
import org.apache.http.util.EntityUtils;
import org.json.JSONException;
import org.json.JSONObject;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Fragment;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;
import android.util.SparseIntArray;

import com.alipay.android.app.sdk.AliPay;
import com.kii.cloud.storage.Kii;
import com.paypal.android.sdk.payments.PayPalConfiguration;
import com.paypal.android.sdk.payments.PayPalPayment;
import com.paypal.android.sdk.payments.PayPalService;
import com.paypal.android.sdk.payments.PaymentActivity;
import com.paypal.android.sdk.payments.PaymentConfirmation;

/**
 * Created by tian on 3/9/14.
 */
@SuppressLint("NewApi")
public class KiiPayment {

    private static final String TAG = KiiPayment.class.getName();

    public static final int REQUEST_PAYPAL = 0x9323;

    private Activity context;

    private Object startActivityObject;

    private KiiOrder order;

    private KiiPaymentCallback callback;

    private boolean sandbox_mode = false;

    private String payid = null;

    public KiiPayment(Activity activity, KiiOrder order, KiiPaymentCallback callback) {
        this.context = activity;
        this.order = order;
        this.callback = callback;
        this.startActivityObject = activity;
        init();
    }

    public KiiPayment(Fragment fragment, KiiOrder order, KiiPaymentCallback callback) {
        this.context = fragment.getActivity();
        this.order = order;
        this.callback = callback;
        this.startActivityObject = fragment;
        init();
    }

    public KiiPayment(android.support.v4.app.Fragment fragment, KiiOrder order,
            KiiPaymentCallback callback) {
        this.context = fragment.getActivity();
        this.order = order;
        this.callback = callback;
        this.startActivityObject = fragment;
        init();
    }

    private void init() {
        Utils.setContextRef(context);
        if (sandbox_mode && order.payType.contentEquals(KiiOrder.ALIPAY)) {
            order.price = 0.01;
        }
    }

    public void enableSandboxMode(boolean value) {
        sandbox_mode = value;
    }

    public boolean isInSandbox() {
        return sandbox_mode;
    }

    public void pay() {
        Currency currency = order.currency;
        if (currency.getCurrencyCode().contentEquals(
                Currency.getInstance(Locale.CHINA).getCurrencyCode())) {
            order.payType = KiiOrder.ALIPAY;
        } else {
            order.payType = KiiOrder.PAYPAL;
        }
        getTransactionFromServer();
    }

    private ProgressDialog progressDialog;

    private void getTransactionFromServer() {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(context);
        }
        progressDialog.setCancelable(false);
        progressDialog.setIndeterminate(true);
        progressDialog.setTitle(Utils.getStringResId("fetching_transaction_from_server"));
        progressDialog.show();
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    HttpClient client = new DefaultHttpClient();
                    HttpPost request = new HttpPost(Constants.PLATFORM_API_URL);
                    Utils.log(TAG, "get params from server: " + request.getURI());
                        String signature = buildTransactionSignature();
                        List<NameValuePair> nameValuePairs = new ArrayList<NameValuePair>();
                        nameValuePairs.add(new BasicNameValuePair("app_id", Kii.getAppId()));
                        nameValuePairs.add(new BasicNameValuePair("method",
                                "get_transaction_params"));
                        nameValuePairs.add(new BasicNameValuePair("pay_type", order.payType));
                        nameValuePairs.add(new BasicNameValuePair("price", sandbox_mode
                                && order.payType.contentEquals(KiiOrder.ALIPAY) ? "0.01"
                                : Double.toString(order.price)));
                        nameValuePairs.add(new BasicNameValuePair("product_id", order.productId));
                        nameValuePairs
                                .add(new BasicNameValuePair("product_name", order.productName));
                        nameValuePairs.add(new BasicNameValuePair("is_sandbox", sandbox_mode ? "1"
                                : "0"));
                        nameValuePairs.add(new BasicNameValuePair("sign", signature));
                        nameValuePairs.add(new BasicNameValuePair("user_id", order.userId));
                        Utils.log(TAG, "post parameter is " + EntityUtils
                                .toString(new UrlEncodedFormEntity(nameValuePairs)));
                        request.setEntity(new UrlEncodedFormEntity(nameValuePairs, "UTF-8"));
                        HttpResponse response = client.execute(request);
                        String result = EntityUtils.toString(response.getEntity());
                        Utils.log(TAG, "response code is "
                                + response.getStatusLine().getStatusCode());
                        Utils.log(TAG, "result is " + result);
                        Message msg = handler.obtainMessage(MSG_GET_PARAMS);
                        msg.obj = result;
                        handler.sendMessage(msg);
                        return;
                } catch (Exception e) {
                    e.printStackTrace();
                    callback.onError(KiiPaymentResultCode.ERROR_NETWORK_EXCEPTION);
                }
            }
        }).start();
    }

    private String buildTransactionSignature() {
        StringBuilder sb = new StringBuilder();
        sb.append("app_id");
        sb.append(Kii.getAppId());
        sb.append("is_sandbox");
        sb.append(sandbox_mode ? 1 : 0);
        sb.append("method");
        sb.append("get_transaction_params");
        sb.append("pay_type");
        sb.append(order.payType);
        sb.append("price");
        sb.append(sandbox_mode && order.payType.contentEquals(KiiOrder.ALIPAY) ? "0.01"
                : order.price);
        sb.append("product_id");
        sb.append(order.productId);
        sb.append("product_name");
        sb.append(order.productName);
        sb.append("user_id");
        sb.append(order.userId);
        return KiiStore.buildHash(sb.toString(), Kii.getAppKey());
    }

    private static final int MSG_GET_PARAMS = 0;

    private static final int MSG_FINISH_TRANSACTION = 1;

    private static final int MSG_ALIPAY_FINISHED = 2;

    private static final int MSG_PAYPAL_FINISHED = 3;

    private static final int MSG_UNION_PAY_FINISHED = 4;

    private static final int MSG_PAY_FAILED = 5;

    private String transactionId;

    private String alipayPublicKey;

    private Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_GET_PARAMS:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    String result = (String) msg.obj;
                    try {
                        JSONObject object = new JSONObject(result);
                        if (object.has("code")) {
                            callback.onError(object.getInt("code"));
                            return;
                        }
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                    if (order.payType.contentEquals(KiiOrder.UNION_PAY)) {
                        // TODO
                    } else if (order.payType.contentEquals(KiiOrder.ALIPAY)) {
                        payWithAlipay(msg);
                    } else if (order.payType.contentEquals(KiiOrder.PAYPAL)) {
                        payWithPaypal(msg);
                    }
                    break;
                case MSG_FINISH_TRANSACTION:
                    if (progressDialog != null) {
                        progressDialog.dismiss();
                    }
                    try {
                        String data = (String) msg.obj;
                        JSONObject object = new JSONObject(data);
                        if (object.getBoolean("result")) {
                            callback.onSuccess();
                        } else {
                            callback.onError(object.getInt("code"));
                        }
                    } catch (Exception e) {
                        e.printStackTrace();
                        callback.onError(KiiPaymentResultCode.ERROR_NETWORK_EXCEPTION);
                    }
                    break;
                case MSG_ALIPAY_FINISHED:
                    handleAlipayFinishStatus(msg);
                    break;
                case MSG_PAYPAL_FINISHED:
                    break;
                case MSG_UNION_PAY_FINISHED:
                    break;
                case MSG_PAY_FAILED:
                    callback.onError(KiiPaymentResultCode.ERROR_UNKNOWN_ERROR);
                    break;
            }
        }
    };

    private void handleAlipayFinishStatus(Message msg) {
        String strRet = (String) msg.obj;
        Utils.log(TAG, "handleAlipayFinishedStatus, strRet is " + strRet);
        try {
            String tradeStatus = "resultStatus={";
            int imemoStart = strRet.indexOf("resultStatus=");
            imemoStart += tradeStatus.length();
            int imemoEnd = strRet.indexOf("}", imemoStart);
            tradeStatus = strRet.substring(imemoStart, imemoEnd);
            if (tradeStatus.equals("9000")) {
                finishTransactionAsyncTask();
            } else {
                if (callback != null) {
                    int alipayErrorCode = 0;
                    try {
                        alipayErrorCode = Integer.parseInt(tradeStatus);
                    } catch (Exception e) {
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
        }

    }

    private void payWithAlipay(Message msg) {
        String info = null;
        if (msg.obj != null) {
            String data = (String) msg.obj;
            try {
                Utils.log(TAG, "data is " + data);
                JSONObject object = new JSONObject(data);
                transactionId = object.getString("transaction_id");
                alipayPublicKey = object.getString("alipay_public_key");
                String partner = object.getString("alipay_partner_id");
                String sellerId = object.getString("alipay_seller_id");
                info = buildAliPayOrder(order.subject, order.body, Double
                        .toString(order.price), transactionId, partner, sellerId);
                String privateKey = object.getString("alipay_rsa_signature");
                String sign = Rsa.sign(info, privateKey);
                sign = URLEncoder.encode(sign);
                info += "&sign=\"" + sign + "\"&" + getAliPaySignType();
            } catch (JSONException e) {
                e.printStackTrace();
                handler.sendEmptyMessage(MSG_PAY_FAILED);
            }
        } else {
            handler.sendEmptyMessage(MSG_PAY_FAILED);
        }
        final String orderInfo = info;
        new Thread() {
            public void run() {
                if (orderInfo == null) {
                    // TODO: send error message
                    return;
                }
                Utils.log(TAG, "orderInfo is " + orderInfo);
                AliPay alipay = new AliPay(context, handler);
                String result = alipay.pay(orderInfo);
                Message msg = new Message();
                msg.what = MSG_ALIPAY_FINISHED;
                Utils.log(TAG, "result is " + result);
                msg.obj = result;
                handler.sendMessage(msg);
            }
        }.start();
    }

    private static String buildAliPayOrder(String subject, String body, String price,
            String tradeNo, String partner,
            String sellerId) {
        StringBuilder sb = new StringBuilder();
        sb.append("partner=\"");
        sb.append(partner);
        sb.append("\"&out_trade_no=\"");
        sb.append(tradeNo);
        sb.append("\"&subject=\"");
        sb.append(subject);
        sb.append("\"&body=\"");
        sb.append(body);
        sb.append("\"&total_fee=\"");
        sb.append(price);
        sb.append("\"&notify_url=\"");

        // 网址需要做URL编码
        sb.append(URLEncoder.encode(Utils.getNotifyUrl()));
        sb.append("\"&service=\"mobile.securitypay.pay");
        sb.append("\"&_input_charset=\"UTF-8");
        sb.append("\"&return_url=\"");
        sb.append(URLEncoder.encode("http://m.alipay.com"));
        sb.append("\"&payment_type=\"1");
        sb.append("\"&seller_id=\"");
        sb.append(sellerId);

        // 如果show_url值为空，可不传
        // sb.append("\"&show_url=\"");
        sb.append("\"&it_b_pay=\"1m");
        sb.append("\"");

        return new String(sb);
    }

    private static String getAliPaySignType() {
        return "sign_type=\"RSA\"";
    }

    private void finishTransactionAsyncTask() {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(context);
        }
        progressDialog.setCancelable(false);
        progressDialog.setIndeterminate(true);
        progressDialog.setTitle(Utils.getStringResId("finishing_transaction"));
        progressDialog.show();
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    finishTransaction();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }).start();
    }

    private void finishTransaction() {
        String result = null;
        try {
            HttpClient client = new DefaultHttpClient();
            HttpPost request = new HttpPost(Constants.PLATFORM_API_URL);
            Utils.log(TAG, "finish order: " + request.getURI());
            String signature = buildFinishSignature();
            List<NameValuePair> nameValuePairs = new ArrayList<NameValuePair>();
            nameValuePairs.add(new BasicNameValuePair("app_id", Kii.getAppId()));
            nameValuePairs.add(new BasicNameValuePair("method", "finished_order"));
            nameValuePairs.add(new BasicNameValuePair("is_sandbox", sandbox_mode ? "1" : "0"));
            nameValuePairs.add(new BasicNameValuePair("transaction_id", transactionId));
            nameValuePairs.add(new BasicNameValuePair("sign", signature));
            if (!TextUtils.isEmpty(payid)) {
                nameValuePairs.add(new BasicNameValuePair("payid", payid));
            }
            request.setEntity(new UrlEncodedFormEntity(nameValuePairs));
            Utils.log(
                    TAG,
                    "finish order, entity is "
                            + EntityUtils.toString(new UrlEncodedFormEntity(nameValuePairs)));
            HttpResponse response = client.execute(request);
            result = EntityUtils.toString(response.getEntity());
            Utils.log(TAG, "response code is " + response.getStatusLine().getStatusCode());
            Utils.log(TAG, "result is " + result);
        } catch (Exception e) {
            e.printStackTrace();
        }
        Message msg = handler.obtainMessage(MSG_FINISH_TRANSACTION);
        msg.obj = result;
        handler.sendMessage(msg);
    }

    private String buildFinishSignature() {
        StringBuilder sb = new StringBuilder();
        sb.append("app_id");
        sb.append(Kii.getAppId());
        sb.append("is_sandbox");
        sb.append(sandbox_mode ? 1 : 0);
        sb.append("method");
        sb.append("finished_order");
        if (!TextUtils.isEmpty(payid)) {
            sb.append("payid");
            sb.append(payid);
        }
        sb.append("transaction_id");
        sb.append(transactionId);
        return KiiStore.buildHash(sb.toString(), Kii.getAppKey());
    }

    private void payWithPaypal(Message msg) {
        String s = (String) msg.obj;
        Log.e(TAG, s);
        String CONFIG_CLIENT_ID = null;
        try {
            JSONObject json = new JSONObject(s);
            transactionId = json.getString("transaction_id");
            if (sandbox_mode) {
                CONFIG_CLIENT_ID = json.optString("paypal_sandbox_client_id");
            } else {
                CONFIG_CLIENT_ID = json.optString("paypal_client_id");
            }
        } catch (JSONException e) {
        }
        PayPalPayment thingToBuy = new PayPalPayment(new BigDecimal(order.price), order.currency
                .getCurrencyCode(), transactionId, PayPalPayment.PAYMENT_INTENT_SALE);
        PayPalConfiguration config = new PayPalConfiguration();
        if (sandbox_mode) {
            config.environment(PayPalConfiguration.ENVIRONMENT_SANDBOX);
        } else {
            config.environment(PayPalConfiguration.ENVIRONMENT_PRODUCTION);
        }
        config.clientId(CONFIG_CLIENT_ID);
        Intent intent = new Intent(context, PayPalService.class);
        intent.putExtra(PayPalService.EXTRA_PAYPAL_CONFIGURATION, config);
        context.startService(intent);
        intent = new Intent(context, PaymentActivity.class);
        intent.putExtra(PaymentActivity.EXTRA_PAYMENT, thingToBuy);
        if (startActivityObject instanceof Activity) {
            ((Activity) startActivityObject).startActivityForResult(intent, REQUEST_PAYPAL);
        } else if (startActivityObject instanceof Fragment) {
            ((Fragment) startActivityObject).startActivityForResult(intent, REQUEST_PAYPAL);
        } else if (startActivityObject instanceof android.support.v4.app.Fragment) {
            ((android.support.v4.app.Fragment) startActivityObject).startActivityForResult(intent,
                    REQUEST_PAYPAL);
        }
    }

    public boolean handleActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_PAYPAL) {
            handlePaypalActivityResult(requestCode, resultCode, data);
            return true;
        }
        return false;
    }

    private void handlePaypalActivityResult(int requestCode, int resultCode, Intent data) {

        if (resultCode == Activity.RESULT_OK) {
            context.stopService(new Intent(context, PayPalService.class));
            PaymentConfirmation confirm = data
                    .getParcelableExtra(PaymentActivity.EXTRA_RESULT_CONFIRMATION);
            if (confirm != null) {
                try {
                    JSONObject json = confirm.toJSONObject();
                    Log.i(TAG, json.toString(4));
                    payid = json.getJSONObject("response").getString("id");
                    finishTransactionAsyncTask();
                } catch (JSONException e) {
                    Log.e(TAG, "an extremely unlikely failure occurred: ", e);
                }
            }
        } else if (resultCode == Activity.RESULT_CANCELED) {
            Log.i(TAG, "The user canceled.");
        } else if (resultCode == PaymentActivity.RESULT_EXTRAS_INVALID) {
            Log.i(TAG, "An invalid payment was submitted. Please see the docs.");
        }
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
        }
    }

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

}
