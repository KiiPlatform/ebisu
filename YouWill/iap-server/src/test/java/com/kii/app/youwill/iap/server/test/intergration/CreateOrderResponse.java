package com.kii.app.youwill.iap.server.test.intergration;

import com.kii.app.youwill.iap.server.entity.PayType;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-8-27.
 */
public class CreateOrderResponse {

	private String transactionID;

	private AlipayPayInfo aliPayInfo;

	private PaypalPayInfo  paypalInfo;


	public CreateOrderResponse(JSONObject json) throws JSONException {

		transactionID=json.getString("transactionID");

		PayType type=PayType.valueOf(json.getString("payType"));

		JSONObject payInfo=json.getJSONObject("payInfo");

		switch(type){
			case alipay:aliPayInfo=new AlipayPayInfo(payInfo);break;
			case paypal:paypalInfo=new PaypalPayInfo(payInfo);break;
		}


	}

	public String getTransactionID() {
		return transactionID;
	}

	public AlipayPayInfo getAliPayInfo() {
		return aliPayInfo;
	}

	public PaypalPayInfo getPaypalInfo() {
		return paypalInfo;
	}
}
