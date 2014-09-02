package com.kii.payment;

import java.util.Currency;
import java.util.Locale;

import org.json.JSONException;
import org.json.JSONObject;

import com.kii.cloud.storage.KiiUser;

/**
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

    public String getPayType() {
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

    String payType;

    int consumeType;

    String productName;

    String productDescription;

    public static final String ALIPAY = "alipay";

    public static final String PAYPAL = "paypal";

    public static final String UNION_PAY = "unionpay";

    @Override
    public String toString() {
        return "id: " + id + ", subject: " + subject + ", body: " + body + ", price:" + price + ", " +
                "productId: " + productId + ", userId: " + userId + ", currency: " + currency;
    }

    public KiiOrder(String string) {
        try {
            JSONObject object = new JSONObject(string);
            subject = object.optString("subject");
            body = object.optString("body");
            price = object.optInt("price");
            id = object.optString("id");
            productId = object.optString("product_id");
            userId = object.optString("user_id");
            currency = Currency.getInstance(object.optString("currency"));
            payType = object.optString("pay_type");
            consumeType = object.optInt("consume_type");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public KiiOrder(KiiProduct product, KiiUser user) {
        subject = product.getName();
        body = product.getDescription();
        price = product.getPrice();
        productId = product.getId();
        userId = user.toUri().toString();
        currency = product.getCurrency();
        productDescription = product.getLocalizedDescription();
        consumeType = product.getConsumeType().ordinal();
    }


    public KiiOrder(KiiProduct product, KiiUser user, Locale locale) {
        subject = product.getLocalizedName(locale);
        body = product.getLocalizedDescription(locale);
        price = product.getPrice();
        productId = product.getId();
        userId = user.toUri().getLastPathSegment();
        currency = product.getCurrency();
        consumeType = product.getConsumeType().ordinal();
        productName = product.getLocalizedName(locale);
        productDescription = product.getLocalizedDescription(locale);
    }

    public KiiOrder() {

    }

    public static class Builder {

        private KiiOrder order;

        public Builder() {
            order = new KiiOrder();
        }

        public Builder setSubject(String subject) {
            order.subject = subject;
            return this;
        }

        public Builder setBody(String body) {
            order.body = body;
            return this;
        }

        public Builder setPrice(int price) {
            order.price = price;
            return this;
        }

        public Builder setId(String id) {
            order.id = id;
            return this;
        }

        public Builder setUserId(String userId) {
            order.userId = userId;
            return this;
        }

        public Builder setCurrency(Currency currency) {
            order.currency = currency;
            return this;
        }

        public Builder setPayType(String payType) {
            order.payType = payType;
            return this;
        }

        public KiiOrder create() {
            return order;
        }

    }
}
