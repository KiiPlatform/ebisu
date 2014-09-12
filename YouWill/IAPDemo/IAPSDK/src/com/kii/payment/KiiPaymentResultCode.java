package com.kii.payment;

/**
 * Created by tian on 2/27/14.
 */
public final class KiiPaymentResultCode {
    //error code from alipay
    public static final int SUCCESS = 9000;
    public static final int ERROR_SYSTEM_EXCEPTION = 1000;
    public static final int ERROR_DATA_FORMAT = 1001;
    public static final int ERROR_ACCOUNT_FROZEN = 1002;
    public static final int ERROR_ACCOUNT_NOT_BIND = 1003;
    public static final int ERROR_BIND_FAILED = 1004;
    public static final int ERROR_PAY_FAILED = 1005;
    public static final int ERROR_ACCOUNT_NEED_REBIND = 1006;
    public static final int ERROR_SERVER_UPGRADING = 1007;
    public static final int ERROR_PAYMENT_CANCELLED = 1008;
    public static final int ERROR_NETWORK_EXCEPTION = 1009;
    public static final int ERROR_INVALID_ORDER = 2001;
    public static final int ERROR_UNKNOWN_ERROR = -1;
    
    //error code from server
    public static final int METHOD_ERROR = 100;
    public static final int PARAMETER_ERROR = 101;
    public static final int PARAMETER_ERROR2 = 102;
    public static final int PARAMETER_ERROR3 = 103;
    public static final int METHOD_ERROR2 = 104;
    public static final int PRODUCT_NOT_FOUND = 105;
    public static final int PRODUCT_ALREADY_BOUGHT = 106;
    public static final int PARAMETER_ERROR4 = 107;
    public static final int ALIPAY_NOT_SUPPORTED = 108;
    public static final int PAYPAL_NOT_SUPPORTED = 109;
    public static final int UNKNOWN_PAYMENT_METHOD = 110;
    public static final int CREATE_ORDER_ERROR = 111;
    public static final int ORDER_DOES_NOT_EXIST = 112;
    public static final int UPDATE_ORDER_FAILED = 113;
    public static final int CREATE_RECEIPT_FAILED = 114;
    public static final int INVALID_PAYPAL_PARAMETER = 115;
    public static final int INVALID_PAYPAL_PARAMETER2 = 116;
    public static final int PAYPAL_VERIFY_FAILED = 117;

    public static final int ALIPAY_VERIFY_FAILED = 200;
}