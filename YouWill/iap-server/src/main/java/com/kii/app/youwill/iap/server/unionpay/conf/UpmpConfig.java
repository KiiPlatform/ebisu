package com.kii.app.youwill.iap.server.unionpay.conf;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import java.io.InputStream;
import java.util.PropertyResourceBundle;

/**
 * Created by liangyx on 12/10/14.
 */
public class UpmpConfig {

    public static String VERSION = "1.0.0";

    public static String CHARSET = "UTF-8";

    public static String TRADE_URL = "http://202.101.25.178:8080/gateway/merchant/trade";

    public static String QUERY_URL = "http://202.101.25.178:8080/gateway/merchant/query";

    public static String MER_ID = "880000000001696";

    public static String MER_BACK_END_URL = "http://118.102.25.205:8080/iap-webapp/iap/callback/unionpay";

    //public static String MER_FRONT_END_URL = "http://www.yourdomain.com/your_path/yourBackEndUrl";

    public static String MER_FRONT_RETURN_URL;

    public static String SIGN_TYPE = "MD5";

    public static String SECURITY_KEY = "pSbz7fcwVc8iZ5rfv8b4ZC9ZAn2Oy2be";

    public static String TRANS_TYPE = "01";

    private static final String KEY_VERSION = "version";
    private static final String KEY_CHARSET = "charset";
    private static final String KEY_TRADE_URL = "upmp.trade.url";
    private static final String KEY_QUERY_URL = "upmp.query.url";
    private static final String KEY_MER_ID = "mer.id";
    private static final String KEY_MER_BACK_END_URL = "mer.back.end.url";
    private static final String KEY_MER_FRONT_END_URL = "mer.front.end.url";
    private static final String KEY_SIGN_METHOD = "sign.method";
    private static final String KEY_SECURITY_KEY = "security.key";

    // 成功应答码
    public static final String RESPONSE_CODE_SUCCESS = "00";

    // 签名
    public final static String SIGNATURE = "signature";

    // 签名方法
    public final static String SIGN_METHOD = "signMethod";

    // 应答码
    public final static String RESPONSE_CODE = "respCode";

    // 应答信息
    public final static String RESPONSE_MSG = "respMsg";

}
