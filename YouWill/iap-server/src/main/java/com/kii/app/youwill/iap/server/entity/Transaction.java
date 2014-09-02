package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.platform.ufp.user.UserID;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import java.util.Date;
import java.util.HashMap;
import java.util.Map;

/**
 * Created by ethan on 14-7-25.
 */
public class Transaction extends KiiEntity {

    /*
	transaction_id
product_id
user_id
transaction_type
transaction_status
pay_status
product_name
price
transaction_created
transaction_updated
pay_completedate
is_sandbox
     */

	public Map<String,Object> getPaymentUpdate() {

		Map<String,Object> map=new HashMap<String,Object>();
		map.put("payStatus", this.getPayStatus().name());
		map.put("payCompleteDate", this.getPayCompleteDate());

		return map;
	}

	public JSONObject getJsonObject() {


		try{
			JSONObject json=new JSONObject();

			json.put("transactionID",this.getTransactionID());
			json.put("price",this.getPrice());
			json.put("userID",this.getUserID());
			json.put("payStatus",this.getPayStatus().name());
			json.put("createdDate",this.getCreatedDate().getTime());

			if(this.getModifyDate()!=null) {
				json.put("updatedDate", this.getModifyDate().getTime());
			}
			if(this.getPayCompleteDate()!=null) {
				json.put("payCompleteDate", this.getPayCompleteDate().getTime());
			}
			json.put("payType",this.getPayType().name());
			if(this.isSandBox()) {
				json.put("isSandbox", this.isSandBox());
			}

			json.put("currency",this.getCurrency().name());
			json.put("productID",this.getProductID());

			return json;
		} catch (JSONException e) {
			throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
		}
	}

	public Transaction(Product product,UserID userID,CurrencyType currency){
		this.productID=product.getProductID();
		this.transactionStatus=OrderStatus.pending;
		this.payStatus=OrderStatus.pending;
		this.createdDate=new Date();

		this.userID=userID.toString();
		this.currency=currency;

	}

	public Transaction(JSONObject json) {

		super(json);

		try {
			this.transactionID = json.getString("transactionID");
			this.productID = json.getString("productID");
			this.userID = json.getString("userID");
			this.payStatus = OrderStatus.valueOf(json.getString("payStatus"));
			this.price = json.getString("price");

			this.createdDate = new Date(json.getLong("createdDate"));
			if(json.has("updateDate")) {
				this.modifyDate = new Date(json.getLong("updatedDate"));
			}
			if(json.has("payCompleteDate")) {
				this.payCompleteDate = new Date(json.getLong("payCompleteDate"));
			}
			if(json.has("isSandBox")) {
				this.isSandBox = json.getBoolean("isSandbox");
			}
			if(json.has("payType")) {
				this.payType = PayType.valueOf(json.getString("payType"));
			}
			this.currency=CurrencyType.valueOf(json.getString("currency"));

		} catch (JSONException e) {
			throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
		}
	}


	private String transactionID;

	private String productID;

	private String userID;

	private OrderStatus transactionStatus;

	private OrderStatus payStatus;

	private CurrencyType currency;

	private String price;

	private Date createdDate;

	private Date modifyDate;

	private Date payCompleteDate;

	private boolean isSandBox;

	private PayType payType;


	public String getPrice() {
		return price;
	}

	public Date getCreatedDate() {
		return createdDate;
	}

	public Date getModifyDate() {
		return modifyDate;
	}

	public Date getPayCompleteDate() {
		return payCompleteDate;
	}

	public boolean isSandBox() {
		return isSandBox;
	}

	public PayType getPayType() {
		return payType;
	}

	public String getTransactionID() {
		return transactionID;
	}

	public String getProductID() {
		return productID;
	}

	public String getUserID() {
		return userID;
	}


	public OrderStatus getTransactionStatus() {
		return transactionStatus;
	}

	public void setPayType(PayType payType) {
		this.payType = payType;
	}

	public OrderStatus getPayStatus() {
		return payStatus;
	}

	public void setSandBox(boolean sandBox) {
		this.isSandBox = sandBox;
	}


	public CurrencyType getCurrency() {
		return currency;
	}

	public void setPrice(String price) {
		this.price = price;
	}


	public void setTransactionID(String transactionID) {
		this.transactionID = transactionID;
	}
}
