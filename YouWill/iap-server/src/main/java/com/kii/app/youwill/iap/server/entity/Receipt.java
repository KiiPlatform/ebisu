package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by ethan on 14-7-25.
 */
public class Receipt extends KiiEntity {

/*
String transaction_id;   	 //should be same with the transaction_id in KiiOrder
double price;
String currency;
String product_name;   	 //same with the product_name field in KiiOrder;
String product_id;
String description;
int consume_type;
*/

    private static String[] ADDITIONAL_FIELDS = new String[]{
            "discount",
            "payment_type",
            "subject",
            //trade_no
            "buyer_email",
            "gmt_create",
            //notify_type
            "quantity",
            //out_trade_no
            "seller_id",
            //notify_time
            "body",
            "trade_status",
            "is_total_fee_adjust",
            "total_fee",
            "gmt_payment",
            "seller_email",
            "gmt_close",
            //price
            "buyer_id",
            //notify_id
            "use_coupon",

            //
            "authorID",
            "appID",
    };

    public Receipt() {

    }

    public JSONObject getJsonObject() {
        try {
            JSONObject obj = null;
            if (additionalFields != null) {
                obj = new JSONObject(additionalFields);
            } else {
                obj = new JSONObject();
            }
            obj.put("transactionID", this.getTransactionID());
            obj.put("price", this.getPrice());
            obj.put("currency", this.getCurrency().name());
            obj.put("productID", this.getProductID());
            if (isSandbox) {
                obj.put("isSandbox", this.isSandbox);
            }
            obj.put("paymentID", this.paymentID);
            if (app != null) {
                obj.put("app", app);
            }
            return obj;
        } catch (JSONException e) {
            throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
        }

    }

    public Receipt(JSONObject json) {
        super(json);

        try {
            this.transactionID = json.getString("transaction_id");
            this.price = json.getString("price");
            this.currency = CurrencyType.valueOf(json.getString("currency"));
            this.productID = json.getString("product_id");
            if (json.has("isSandbox")) {
                this.isSandbox = json.getBoolean("isSandbox");
            }

        } catch (JSONException e) {
            throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
        }
    }


    public Receipt(Transaction transaction, AlipayQueryResult alipay) {
        this.transactionID = transaction.getTransactionID();
        this.price = alipay.getTotalFee();
        this.currency = transaction.getCurrency();
        this.productID = transaction.getProductID();
        if (transaction.isSandBox()) {
            this.isSandbox = transaction.isSandBox();
        }
        this.paymentID = alipay.getTradeID();

    }


    public Receipt(Transaction transaction, PaypalQueryResult paypal) {
        this.transactionID = transaction.getTransactionID();
        this.price = paypal.getPrice();
        this.currency = transaction.getCurrency();
        this.productID = transaction.getProductID();
        if (transaction.isSandBox()) {
            this.isSandbox = transaction.isSandBox();
        }
        this.paymentID = paypal.getId();


    }

    public Receipt(Transaction transaction, Map<String, String> callbackParams) {
        this.transactionID = transaction.getTransactionID();
        if (transaction.getApp() != null) {
            this.app = transaction.getApp();
        }
        this.price = callbackParams.get("price");
        this.currency = CurrencyType.CNY;
        this.productID = transaction.getProductID();
        if (transaction.isSandBox()) {
            this.isSandbox = transaction.isSandBox();
        }
        this.paymentID = callbackParams.get("trade_no");

        additionalFields = new HashMap<String, String>();

        for (String key : ADDITIONAL_FIELDS) {
            additionalFields.put(key, callbackParams.get(key));
        }
    }

    private String transactionID;

    private String price;

    private CurrencyType currency;


    private String productID;

    private boolean isSandbox;

    private String paymentID;

    private String app;

    private Map<String, String> additionalFields;


    public String getPrice() {
        return price;
    }

    public void setPrice(String price) {
        this.price = price;
    }

    public String getTransactionID() {
        return transactionID;
    }

    public void setTransactionID(String transactionID) {
        this.transactionID = transactionID;
    }

    public CurrencyType getCurrency() {
        return currency;
    }

    public void setCurrency(CurrencyType currency) {
        this.currency = currency;
    }

    public String getProductID() {
        return productID;
    }

    public void setProductID(String productID) {
        this.productID = productID;
    }
}
