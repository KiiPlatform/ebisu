package com.kii.app.youwill.iap.server.entity;

import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-8-14.
 */
public class PaypalConfig {

	private final String clientID;

	private final String secret;

	public PaypalConfig(String clientID,String secret){
		this.clientID=clientID;
		this.secret=secret;
	}

	public JSONObject getPaypayJson(){

		JSONObject json=new JSONObject();
		try {
			json.append("clientID", clientID);
			json.append("secret", secret);
		}catch(JSONException e){
			throw new IllegalArgumentException(e);
		}
		return json;

	}

	public String getClientID() {
		return clientID;
	}

	public String getSecret() {
		return secret;
	}
}
