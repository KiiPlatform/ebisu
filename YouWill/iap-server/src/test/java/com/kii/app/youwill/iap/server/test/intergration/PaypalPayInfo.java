package com.kii.app.youwill.iap.server.test.intergration;

import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-8-27.
 */
public class PaypalPayInfo {

	private String clientID;

	private String secret;

	public PaypalPayInfo(JSONObject json) throws JSONException {
		clientID=json.getString("clientID");
		secret=json.getString("secret");

	}

	public String getClientID() {
		return clientID;
	}

	public String getSecret() {
		return secret;
	}

	@Override
	public String toString(){

		return "clientID:"+clientID+"\n secret:"+secret;

	}
}
