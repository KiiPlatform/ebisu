package com.kii.app.youwill.iap.server.test.intergration;

import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-8-27.
 */
public class AlipayPayInfo {

	/*
	String[]{"alipaySellerID","alipayPartnerID",
					"alipayRSAPrivateKey","alipaySecurityKey"});
	 */

	private String sellerID;

	private String partnerID;

	private String rsaPrivateKey;

	private String securityKey;

	public AlipayPayInfo(JSONObject json) throws JSONException {


		sellerID=json.getString("alipaySellerID");
		partnerID=json.getString("alipayPartnerID");

		if(json.has("alipayRSAPrivateKey")) {
			rsaPrivateKey = json.getString("alipayRSAPrivateKey");
		}
		securityKey=json.getString("alipaySecurityKey");


	}

	@Override
	public String toString(){

		return "partnerID:"+partnerID+"\n sellerID:"+sellerID+" \n RSAPK & DSAPK... ";

	}

	public String getSellerID() {
		return sellerID;
	}

	public String getPartnerID() {
		return partnerID;
	}

	public String getRsaPrivateKey() {
		return rsaPrivateKey;
	}

	public String getSecurityKey() {
		return securityKey;
	}
}
