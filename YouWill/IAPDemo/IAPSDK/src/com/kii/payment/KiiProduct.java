package com.kii.payment;

import android.util.Log;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.KiiUser;

import java.util.Currency;
import java.util.Locale;

/**
 * Created by tian on 14-2-26.
 */
public class KiiProduct {

    private KiiObject mObject;

    public KiiProduct(KiiObject object) {
        mObject = object;
    }

    public String getId() {
        return (mObject != null) ? mObject.toUri().getLastPathSegment() : null;
    }

    public String getName() {
        return (mObject != null && mObject.has("name")) ? mObject.getString("name") : null;
    }

    public String getDescription() {
        return (mObject != null && mObject.has("description")) ? mObject.getString("description") : null;
    }

    public String getThumbnail() {
        return (mObject != null && mObject.has("image")) ? mObject.getString("image") : null;
    }

    public boolean isNew() {
        return (mObject != null && mObject.has("isNew") && mObject.getBoolean("isNew"));
    }

    public double getPrice() {
        return (mObject != null && mObject.has("price")) ? mObject.getDouble("price") : 0;
    }

    public enum CONSUME_TYPE {
        none, permanent, consumable, periodical;
    }

    public CONSUME_TYPE getConsumeType() {
        try {
            return CONSUME_TYPE.valueOf(mObject.getString("consumeType"));
        } catch (Exception e) {
            e.printStackTrace();
        }
        return CONSUME_TYPE.none;

    }

    /**
     *
     */
    public boolean isPurchased(KiiUser user) {
        if (user == null) {
            return false;
        }
        KiiReceipt receipt = KiiStore.getReceipt(getId(), user);

        return (receipt != null);
    }

}
