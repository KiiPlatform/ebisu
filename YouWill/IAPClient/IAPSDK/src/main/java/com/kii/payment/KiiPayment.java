package com.kii.payment;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.util.SparseArray;
import android.util.SparseIntArray;
import com.kii.payment.impl.PrefUtil;

import java.lang.reflect.Field;

/**
 * Created by tian on 3/9/14.
 * Modified by Richard
 */
@SuppressLint("NewApi")
public abstract class KiiPayment {

    static String STATUS_COMPLETED = "completed";

    Activity context;

    KiiOrder order;

    KiiPaymentCallback callback;

    KiiPayment(Activity activity, KiiOrder order, KiiPaymentCallback callback) {
        this.context = activity;
        this.order = order;
        this.callback = callback;
    }

    /**
     * Clear cached result related to the specified order.
     */
    public void clearCachedResult() {
        PrefUtil.clearCachedResult(context, order.getProductId());
    }

    /**
     * Return the number of retrying to confirm the payment with cloud.
     *
     * @return
     */
    public int getRetryNum() {
        return PrefUtil.getRetryNum(context, order.getProductId());
    }

    public abstract void pay();

    public static KiiPayment getPayment(Activity activity, KiiOrder order, KiiPaymentCallback callback) {
        switch (order.getPayType()) {
            case alipay:
                return new AliPayPayment(activity, order, callback);
            case paypal:
                return null;
            case unionpay:
                return null;
            case mm:
                return new MMPayment(activity, order, callback);
            case egame:
                return null;
            case wo:
                return null;
            case unknown:
                return null;
        }
        return null;
    }

    /**
     * Get detail error information by errorCode.
     *
     * @param context   - application context
     * @param errorCode - the errorCode
     * @return the error message
     */
    public String getErrorMessage(Context context, int errorCode) {
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

    private static final SparseArray<String> sErrorMap = new SparseArray<String>();

    static {
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
            sErrorMap.put(KiiPaymentResultCode.ERROR_MM_INVALID_PAY_CODE, "kii_payment_mm_invalid_pay_code");
        }
    }
}
