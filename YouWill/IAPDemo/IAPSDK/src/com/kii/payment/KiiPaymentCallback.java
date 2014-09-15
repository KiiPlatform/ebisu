package com.kii.payment;

/**
 * Created by tian on 2/27/14.
 */
public interface KiiPaymentCallback {
    /**
     * method called after the payment is successful
     */
    public abstract void onSuccess();

    /**
     * method called when the payment runs into any exceptions.
     *
     * @param errorCode - errorCode, You can get detail error message by KiiPayment.getErrorMessage
     */
    public abstract void onError(int errorCode);
}
