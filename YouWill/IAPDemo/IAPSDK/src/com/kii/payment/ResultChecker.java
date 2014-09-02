package com.kii.payment;

import org.json.JSONObject;

/**
 * 对签名进行验签
 */
class ResultChecker {

    private static final int RESULT_INVALID_PARAM = 0;
    public static final int RESULT_CHECK_SIGN_FAILED = 1;
    private static final int RESULT_CHECK_SIGN_SUCCEED = 2;

    private String mContent;

    public ResultChecker(String content) {
        this.mContent = content;
    }

    /**
     * 对签名进行验签
     *
     * @return
     */
    int checkSign(String publicKey) {
        int retVal = RESULT_CHECK_SIGN_SUCCEED;

        try {
            JSONObject objContent = BaseHelper.string2JSON(this.mContent, ";");
            String result = objContent.getString("result");
            result = result.substring(1, result.length() - 1);
            // 获取待签名数据
            int iSignContentEnd = result.indexOf("&sign_type=");
            String signContent = result.substring(0, iSignContentEnd);
            // 获取签名
            JSONObject objResult = BaseHelper.string2JSON(result, "&");
            String signType = objResult.getString("sign_type");
            signType = signType.replace("\"", "");

            String sign = objResult.getString("sign");
            sign = sign.replace("\"", "");
            // 进行验签 返回验签结果
            if (signType.equalsIgnoreCase("RSA")) {
                if (!Rsa.doCheck(signContent, sign, publicKey)) { retVal = RESULT_CHECK_SIGN_FAILED; }
            }
        } catch (Exception e) {
            retVal = RESULT_INVALID_PARAM;
            e.printStackTrace();
        }

        return retVal;
    }

//	boolean isPayOk() {
//		boolean isPayOk = false;
//
//		String success = getSuccess();
//		if (success.equalsIgnoreCase("true")
//				&& checkSign() == RESULT_CHECK_SIGN_SUCCEED)
//			isPayOk = true;
//
//		return isPayOk;
//	}
}