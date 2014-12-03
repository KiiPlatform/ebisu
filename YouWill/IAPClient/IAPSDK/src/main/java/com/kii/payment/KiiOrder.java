package com.kii.payment;

import com.kii.cloud.storage.KiiUser;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Currency;

/**
 * KiiOrder class
 * <p/>
 * Created by tian on 2/27/14.
 */
public class KiiOrder {
    private static final String TAG = KiiOrder.class.getName();
    //should add other fields for other payment types
    String subject; // no longer than 64;

    public String getSubject() {
        return subject;
    }

    public String getBody() {
        return body;
    }

    public double getPrice() {
        return price;
    }

    public String getId() {
        return id;
    }

    public String getProductId() {
        return productId;
    }

    public String getUserId() {
        return userId;
    }

    public Currency getCurrency() {
        return currency;
    }

    public PayType getPayType() {
        return payType;
    }

    public String getProductName() {
        return productName;
    }

    public String getProductDescription() {
        return productDescription;
    }

    String body; // no longer than 1024;

    double price; // > 0;

    String id; // no longer than 64;

    String productId;

    String userId;

    Currency currency;

    PayType payType;

    int consumeType;

    String productName;

    String productDescription;

    @Override
    public String toString() {
        return "id: " + id + ", subject: " + subject + ", body: " + body + ", price:" + price + ", " +
                "productId: " + productId + ", userId: " + userId + ", currency: " + currency;
    }

    /**
     * Constructs an instance of KiiOrder with the specified order in json string. The json string might contains
     * contains the following fields: subject, body, price, id, product_id, user_id, currency, consume_type.
     *
     * @param order
     */
    public KiiOrder(String order) {
        try {
            JSONObject object = new JSONObject(order);
            subject = object.optString("subject");
            body = object.optString("body");
            price = object.optInt("price");
            id = object.optString("id");
            productId = object.optString("product_id");
            userId = object.optString("user_id");
            currency = Currency.getInstance(object.optString("currency"));
            try {
                payType = PayType.valueOf(object.optString("pay_type"));
            } catch (Exception e) {
                payType = payType.unknown;
            }
            consumeType = object.optInt("consume_type");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    /**
     * Constructs an instance of KiiOrder with the specified KiiProduct and KiiUser.
     *
     * @param product
     * @param user
     */
    public KiiOrder(KiiProduct product, KiiUser user) {
        this(product, user, PayType.alipay);
    }

    public KiiOrder(KiiProduct product, KiiUser user, PayType payType) {
        subject = product.getName();
        body = product.getDescription();
        price = product.getPrice();
        productId = product.getId();
        userId = user.toUri().getLastPathSegment();
        consumeType = product.getConsumeType().ordinal();
        this.payType = payType;
    }

}
