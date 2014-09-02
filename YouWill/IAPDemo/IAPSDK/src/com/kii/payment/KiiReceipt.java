package com.kii.payment;

import com.kii.cloud.storage.KiiObject;

import java.util.Currency;

/**
 * Created by tian on 2/27/14.
 */
public class KiiReceipt {
    private KiiObject mObject;

    public KiiReceipt(KiiObject object) {
        mObject = object;
    }

    public String getId() {
        return (mObject != null) ? mObject.toUri().toString() : null;
    }

    public String getTransactionId() {
        return (mObject != null && mObject.has("transaction_id")) ? mObject.getString("transaction_id") : null;
    }

    public int getQuantity() {
        return (mObject != null && mObject.has("quantity")) ? mObject.getInt("quantity") : 0;
    }

    public double getUnityPrice() {
        return (mObject != null && mObject.has("unity_price")) ? mObject.getInt("unity_price") : 0;
    }

    public double getPrice() {
        return (mObject != null && mObject.has("price")) ? mObject.getInt("price") : 0;
    }

    public Currency getCurrency() {
        return (mObject != null && mObject.has("currency")) ? Currency
                .getInstance(mObject.getString("currency")) : null;
    }

    public String getProductName() {
        return (mObject != null && mObject.has("product_name")) ? mObject.getString("product_name") : null;
    }


    public String getProductId() {
        return (mObject != null && mObject.has("product_id")) ? mObject.getString("product_id") : null;
    }

    public String getDescription() {
        return (mObject != null && mObject.has("description")) ? mObject.getString("description") : null;
    }

    public KiiProduct.CONSUME_TYPE getConsumeType() {
        return (mObject != null && mObject.has("consume_type")) ?
                KiiProduct.CONSUME_TYPE.values()[mObject.getInt("consume_type")] : KiiProduct.CONSUME_TYPE.NONE;
    }

    public long getCreatedTime() {
        return mObject != null ? mObject.getCreatedTime() : -1;
    }
}
