package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.common.IAPUtils;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import java.util.Date;

/**
 * Created by ethan on 14-8-14.
 */
public class PaypalQueryResult {

	/*

	{
  "id": "PAY-5YK922393D847794YKER7MUI",
  "create_time": "2013-02-19T22:01:53Z",
  "update_time": "2013-02-19T22:01:55Z",
  "state": "approved",
  "intent": "sale",

	 */

	private String id;

	private Date createTime;

	private Date updateTime;

	private StateType state;

	private IntentType intent;

	private String price;

	private CurrencyType currency;

	public PaypalQueryResult(JSONObject response){


		try{

			JSONArray payments=response.getJSONArray("payments");

			JSONObject  json=payments.getJSONObject(0);

			id=json.getString("id");
			updateTime= IAPUtils.convertPaypalDate(json.getString("update_time"));
			state=StateType.valueOf(json.getString("state"));
			intent=IntentType.valueOf(json.getString("intent"));

			JSONObject trans=json.getJSONArray("transactions").getJSONObject(0);
			JSONObject amount=trans.getJSONObject("amount");

			price=amount.getString("total");
			currency=CurrencyType.valueOf(amount.getString("currency"));


			/*
			  "transactions": [
    {
      "amount": {
        "total": "7.47",
        "currency": "USD",
        "details": {
          "subtotal": "7.47"
        }
      },
			 */

		}catch(JSONException e){
			e.printStackTrace();
			throw new ServiceException(IAPErrorCode.PAYPAL_SERVICE_INVALID);
		}

	}


	public String getId() {
		return id;
	}

	public Date getCreateTime() {
		return createTime;
	}

	public Date getUpdateTime() {
		return updateTime;
	}

	public StateType getState() {
		return state;
	}

	public IntentType getIntent() {
		return intent;
	}

	public String getPrice() {
		return price;
	}

	public CurrencyType getCurrency() {
		return currency;
	}

	public static enum IntentType {
		sale, authorize, order;
	}

	public static enum StateType{
		created, approved, failed, canceled, expired;
	}
}
