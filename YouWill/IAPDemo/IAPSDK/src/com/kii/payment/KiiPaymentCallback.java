package com.kii.payment;

/**
 * Created by tian on 2/27/14.
 */
public interface KiiPaymentCallback {
    public abstract void onSuccess();
    public abstract void onError(int errorCode);
}
