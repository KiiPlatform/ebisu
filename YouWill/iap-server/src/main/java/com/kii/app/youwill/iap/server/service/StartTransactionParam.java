package com.kii.app.youwill.iap.server.service;

import com.kii.app.youwill.iap.server.entity.PayType;
import org.apache.commons.codec.digest.DigestUtils;
import org.apache.commons.lang3.StringUtils;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-7-28.
 */
public class StartTransactionParam {

	String productID;

	PayType payType;

	String verifySign;

	private String price;

	private String transactionID;


	public StartTransactionParam(String  context) {
		try {
			JSONObject json=new JSONObject(context);

			payType = PayType.valueOf(json.getString("payType"));
			verifySign = json.getString("verifySign");

			price = json.getString("price");


			if (json.has("transactionID")) {
				transactionID = json.getString("transactionID");
			}
		} catch (Exception e) {
			throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
		}
	}

	public void setProductID(String id) {
		this.productID = id;
	}


	public boolean valid(String appID, String secKey) {


		StringBuilder sb = new StringBuilder();

		sb.append("appID").append(appID);
		sb.append("secKey").append(secKey);

		sb.append("productID").append(productID);
		sb.append("payType").append(payType.name());
		if (StringUtils.isNotBlank(price)) {
			sb.append("price").append(price);
		}
		if (StringUtils.isNotBlank(transactionID)) {
			sb.append("transactionID").append(transactionID);
		}


		return  DigestUtils.sha1Hex(sb.toString()).equals(verifySign);
	}

	public String getProductID() {
		return productID;
	}

	public PayType getPayType() {
		return payType;
	}

	public String getPrice() {
		return price;
	}



	public void setPayType(PayType payType) {
		this.payType = payType;
	}


	public String getVerifySign() {
		return verifySign;
	}

	public void setVerifySign(String verifySign) {
		this.verifySign = verifySign;
	}

	public void setPrice(String price) {
		this.price = price;
	}

	public String getTransactionID() {
		return transactionID;
	}

}
