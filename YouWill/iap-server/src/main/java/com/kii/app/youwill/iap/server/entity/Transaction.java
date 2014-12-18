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

    public Map<String, Object> getPaymentUpdate() {

        Map<String, Object> map = new HashMap<String, Object>();
        map.put("payStatus", this.getPayStatus().name());
        map.put("payCompleteDate", this.getPayCompleteDate());

        return map;
    }

    public JSONObject getJsonObject() {


        try {
            JSONObject json = new JSONObject();

            json.put("transactionID", this.getTransactionID());
            json.put("price", this.getPrice());
            json.put("userID", this.getUserID());
            json.put("payStatus", this.getPayStatus().name());
            json.put("createdDate", this.getCreatedDate().getTime());

            if (this.getModifyDate() != null) {
                json.put("updatedDate", this.getModifyDate().getTime());
            }
            if (this.getPayCompleteDate() != null) {
                json.put("payCompleteDate", this.getPayCompleteDate().getTime());
            }
            json.put("payType", this.getPayType().name());
            if (this.isSandBox()) {
                json.put("isSandbox", this.isSandBox());
            }

            json.put("currency", this.getCurrency().name());
            json.put("productID", this.getProductID());

            if (authorID != null) {
                json.put("authorID", authorID);
            }

            if (appID != null) {
                json.put("appID", appID);
            }

            if (app != null) {
                json.put("app", app);
            }

            if (payCode != null) {
                json.put("pay_code", payCode);
            }

            putAdditionalFields(json);
            return json;
        } catch (JSONException e) {
            throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
        }
    }

    /*
    * save additional fields from IAP product. */
    private void putAdditionalFields(JSONObject json) {
        if (product == null) {
            return;
        }

        String[] additionalFields = new String[]{
                "name",
                "developer_id",
                "developer_name",
                "category_id",
                "category_name",
                "recommend_name",
                "recommend_type",
                "recommend_weight",
        };
        for (String field : additionalFields) {
            String value = product.getFieldByName(field);
            if (value != null) {
                try {
                    json.put(field, value);
                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public Transaction(Product product, UserID userID, CurrencyType currency) {
        this.productID = product.getProductID();
        this.app = product.getApp();
        this.transactionStatus = OrderStatus.pending;
        this.payStatus = OrderStatus.pending;
        this.createdDate = new Date();
        this.userID = userID.toString();
        this.currency = currency;
        /* additional fields for analytics */
        this.product = product;


    }

    public Transaction(Product product, UserID userID, String authorID, String appID) {
        this(product, userID, CurrencyType.CNY);
        this.authorID = authorID;
        this.appID = appID;
    }

    public Transaction(JSONObject json) {

        super(json);

        try {
            this.transactionID = json.getString("_id");
            this.productID = json.getString("productID");
            this.userID = json.getString("userID");
            this.payStatus = OrderStatus.valueOf(json.getString("payStatus"));
            this.price = json.getString("price");
            this.appID = json.optString("appID");

            this.createdDate = new Date(json.getLong("createdDate"));
            if (json.has("updateDate")) {
                this.modifyDate = new Date(json.getLong("updatedDate"));
            }
            if (json.has("payCompleteDate")) {
                this.payCompleteDate = new Date(json.getLong("payCompleteDate"));
            }
            if (json.has("isSandBox")) {
                this.isSandBox = json.getBoolean("isSandbox");
            }
            if (json.has("payType")) {
                this.payType = PayType.valueOf(json.getString("payType"));
            }

            if (json.has("app")) {
                this.app = json.getString("app");
            }
            this.currency = CurrencyType.valueOf(json.getString("currency"));

        } catch (JSONException e) {
            throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
        }
    }


    private Product product;

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

    private String authorID;

    private String appID;

    /* the field app is different from appID
     * if the IAP item is an application, then the app is the app_id of the application,
     * while appID means which application the IAP item belongs to.
     *  */
    private String app;

    private String payCode;

    public String getPayCode() {
        return payCode;
    }

    public void setPayCode(String payCode) {
        this.payCode = payCode;
    }

    public String getApp() {
        return app;
    }

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

    public String getAppID() {
        return appID;
    }

    public String getAuthorID() {
        return authorID;
    }
}
