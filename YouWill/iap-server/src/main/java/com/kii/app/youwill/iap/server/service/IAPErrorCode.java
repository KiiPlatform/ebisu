package com.kii.app.youwill.iap.server.service;

/**
* Created by ethan on 14-7-28.
*/
public enum IAPErrorCode {

	FORMAT_INVALID(500),


	APPID_IS_NULL(403),
	APPKEY_IS_NULL(403),
	TOKEN_IS_NULL(403),
	PRODUCT_NOT_FOUND(404),
	PAYID_NOT_FOUND(500),
	ORDER_NOT_FOUND(404),
	USER_NOT_FOUND(500),


	ALIPAY_CONFIG_MISS(500),
	PAYPAL_CONFIG_MISS(500),
	PAYMENT_ID_ERROR(500),
	SECURITY_KEY_MISS(500),

	SIGN_INVALID(500),
	PRODUCT_BOUGHT_ALREADY(500),
	PAY_STATUS_INVALID(500),

	PAYPAL_SERVICE_INVALID(500),
	ALIPAY_SERVICE_INVALID(500),
	;


	int code;

	IAPErrorCode(int val){
		this.code=val;
	}

	public int getCode(){
		return code;
	}

}
