package com.kii.payment;

import com.kii.cloud.storage.KiiObject;

/**
 * This class represents the receipt of a payment given to the user.
 * <p/>
 * Created by tian on 2/27/14.
 */
public class KiiReceipt {

    private KiiObject mObject;

    /**
     * Constructs an KiiReceipt with the specified KiiObject
     *
     * @param object
     */
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

    public String getFieldByName(String fieldName) {
        return (mObject != null && mObject.has(fieldName)) ? mObject.getString(fieldName) : null;
    }
}
