package com.kii.app.youwill.iap.server.service;

import org.apache.commons.codec.digest.DigestUtils;
import org.apache.commons.lang3.StringUtils;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-7-28.
 */
public class FinishTransactionParam {



	private String transactionID;

	private String verifySign;

	private String paymentID;



	public FinishTransactionParam(String context) {
		try {
			JSONObject json=new JSONObject(context);
			if (json.has("paymentID")) {
				paymentID = json.getString("paymentID");
			}

			verifySign = json.getString("verifySign");
		} catch (JSONException e) {
			throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
		}
	}

	public void setTransactionID(String transactionID) {
		this.transactionID = transactionID;
	}


	public boolean valid(String appID, String secKey) {


		StringBuilder sb = new StringBuilder();

		sb.append("appID").append(appID);
		sb.append("secKey").append(secKey);

		sb.append("orderID").append(transactionID);

		if(StringUtils.isNotBlank(paymentID)){
			sb.append("paymentID").append(paymentID);
		}

		return  DigestUtils.sha1Hex(sb.toString()).equals(verifySign);
	}

	public String getTransactionID() {
		return transactionID;
	}

	public String getPaymentID(){
		return this.paymentID;
	}

	public String getVerifySign() {
		return verifySign;
	}

	public void setVerifySign(String verifySign) {
		this.verifySign = verifySign;
	}
}
