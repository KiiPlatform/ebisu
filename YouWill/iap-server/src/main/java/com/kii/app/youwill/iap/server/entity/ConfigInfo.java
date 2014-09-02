package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-7-25.
 */
public class ConfigInfo extends KiiEntity{


	private final JSONObject jsObj;


	public ConfigInfo(JSONObject jsObj)  {
		super(jsObj);

		this.jsObj=jsObj;

	}


	public String getIAPSecurityKey(){
		try {
		return this.jsObj.getString("iapSecurityKey");
		} catch (JSONException e) {
			throw new ServiceException(IAPErrorCode.SECURITY_KEY_MISS);
		}
	}

	public PaypalConfig  getPaypalInfo(){

		try {
			return new PaypalConfig(jsObj.getString("paypalClientID"),jsObj.getString("paypalSecret"));
		} catch (JSONException e) {
			throw new ServiceException(IAPErrorCode.PAYPAL_CONFIG_MISS);
		}

	}

	public PaypalConfig getPaypalSandboxInfo(){
		try {
			return new PaypalConfig(jsObj.getString("paypalSandbox"),jsObj.getString("paypalSandboxSecret"));
		} catch (JSONException e) {
			throw new ServiceException(IAPErrorCode.PAYPAL_CONFIG_MISS);
		}
	}


	public AlipayConfig getAlipayInfo(){

		return new AlipayConfig(jsObj);

	}




}
