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
        return (mObject != null && mObject.has("transactionID")) ? mObject.getString("transactionID") : null;
    }

    public int getQuantity() {
        return (mObject != null && mObject.has("quantity")) ? mObject.getInt("quantity") : 0;
    }

    public double getPrice() {
        return (mObject != null && mObject.has("price")) ? mObject.getInt("price") : 0;
    }

    public Currency getCurrency() {
        return (mObject != null && mObject.has("currency")) ? Currency
                .getInstance(mObject.getString("currency")) : null;
    }

    public String getProductName() {
        return (mObject != null && mObject.has("subject")) ? mObject.getString("subject") : null;
    }


    public String getProductId() {
        return (mObject != null && mObject.has("productID")) ? mObject.getString("productID") : null;
    }

    public String getDescription() {
        return (mObject != null && mObject.has("body")) ? mObject.getString("body") : null;
    }

    public long getCreatedTime() {
        return mObject != null ? mObject.getCreatedTime() : -1;
    }
}
