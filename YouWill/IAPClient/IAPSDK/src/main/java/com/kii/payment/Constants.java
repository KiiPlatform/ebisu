package com.kii.payment;

/**
 * Utility class used to defined all the constants.
 *
 * Created by tian on 13-7-23.
 */
public class Constants {

    //public static final String PLATFORM_EXTENSION_URL = "http://192.168.1.100:8080/api/iap/";
    public static final String PLATFORM_EXTENSION_URL = "http://118.102.25.205:8080/iap-webapp/iap/";
    static final String KEY_APP_DISTRIBUTION_ID = "com.kii.DISTRIBUTION_ID";

    static final class ALIPAY_RESULT_CODE {
        public static final int SUCCESS = 9000;
        public static final int SYSTEM_EXCEPTION = 4000;
        public static final int ERROR_DATA_FORMAT = 4001;
        public static final int ACCOUNT_FROZEN = 4003;
        public static final int ACCOUNT_NOT_BOUND = 4004;
        public static final int ACCOUNT_BIND_FAILED = 4005;
        public static final int PAY_FAILED = 4006;
        public static final int ACCOUNT_NEED_REBIND = 4010;
        public static final int SERVER_UPGRADING = 6000;
        public static final int PAYMENT_CANCELLED = 6001;
        public static final int NETWORK_EXCEPTION = 6002;
    }

    public static final String GET_PARAM_URL = PLATFORM_EXTENSION_URL + "startOrder/product/%s";

    public static final String QUERY_ORDER_URL = PLATFORM_EXTENSION_URL + "queryOrder/order/%s";

    public static final String PLATFORM_CALLBACK_URL_ALIPAY = "http://118.102.25.205:8080/iap-webapp/iap/callback/alipay/%s/app/%s";
    public static final String PLATFORM_CALLBACK_URL_MM = "http://118.102.25.205:8080/iap-webapp/iap/callback/";
}
