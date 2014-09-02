package com.kii.payment;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiBucket;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.exception.app.BadRequestException;
import com.kii.cloud.storage.exception.app.ConflictException;
import com.kii.cloud.storage.exception.app.ForbiddenException;
import com.kii.cloud.storage.exception.app.NotFoundException;
import com.kii.cloud.storage.exception.app.UnauthorizedException;
import com.kii.cloud.storage.exception.app.UndefinedException;
import com.kii.cloud.storage.query.KiiClause;
import com.kii.cloud.storage.query.KiiQuery;
import com.kii.cloud.storage.query.KiiQueryResult;

import org.apache.http.HttpResponse;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
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

    public String getLocalizedName() {
        return getLocalizedName(Locale.getDefault());
    }

    public String getLocalizedName(Locale locale) {
        if (mObject != null && mObject.has("name_" + locale.getCountry())) {
            return mObject.getString("name_" + locale.getCountry());
        }
        return getName();
    }

    public String getDescription() {
        return (mObject != null && mObject.has("description")) ? mObject.getString("description") : null;
    }

    public String getLocalizedDescription() {
        return getLocalizedDescription(Locale.getDefault());
    }

    public String getLocalizedDescription(Locale locale) {
        if (mObject != null && mObject.has("description_" + locale.getCountry())) {
            return mObject.getString("description_" + locale.getCountry());
        }
        return getDescription();
    }

    public String getThumbnail() {
        return (mObject != null && mObject.has("thumbnail")) ? mObject.getString("thumbnail") : null;
    }

    public String getCategory() {
        if (mObject != null && mObject.has("category")) {
            return mObject.getString("category");
        }
        return null;
    }

    public boolean isNew() {
        return (mObject != null && mObject.has("is_new") && mObject.getBoolean("is_new"));
    }

    public boolean isRecommended() {
        return (mObject != null && mObject.has("is_recommended") && mObject.getBoolean("is_recommended"));
    }

    public double getPrice() {
        return (mObject != null && mObject.has("price")) ? mObject.getDouble("price") : 0;
    }

    public Currency getCurrency() {
        return (mObject != null && mObject.has("currency")) ? Currency
                .getInstance(mObject.getString("currency")) : null;
    }

    public int getDiscountPrice() {
        return (mObject != null && mObject.has("discount")) ? mObject.getInt("discount") : -1;
    }

    public int getLocalizedDiscountPrice(Locale locale) {
        if (mObject != null && mObject.has("discount_" + locale.getCountry())) {
            return mObject.getInt("discount_" + locale.getCountry());
        }
        return getDiscountPrice();
    }

    public int getLocalizedDiscountPrice() {
        return getLocalizedDiscountPrice(Locale.getDefault());
    }

    //default valid, invalid on in need
    public boolean isValid() {
        return (mObject != null && mObject.has("valid")) ? mObject.getBoolean("valid") : true;
    }

    public long getCreatedTime() {
        return mObject != null ? mObject.getCreatedTime() : -1;
    }

    public long getLastModifyTime() {
        return mObject != null ? mObject.getModifedTime() : -1;
    }

    public enum CONSUME_TYPE {
        NONE, PERMANENT, CONSUMABLE, PERIODICAL;
    }

    public CONSUME_TYPE getConsumeType() {
        return (mObject != null && mObject.has("consume_type")) ? CONSUME_TYPE.values()[mObject
                .getInt("consume_type")] : CONSUME_TYPE.NONE;
    }

    public int getDisplayOrder() {
        return (mObject != null && mObject.has("display_order")) ? mObject.getInt("display_order") : 0;
    }

    private static final String GET_ORDER_URL = Constants.PLATFORM_EXTENSION_URL + "get_order_by_product.php";


    //will check the status from remote server
    public boolean isProductBought(KiiUser user) throws ForbiddenException, BadRequestException, UndefinedException,
            IOException, ConflictException, NotFoundException, UnauthorizedException, JSONException {
        KiiBucket bucket = Kii.bucket("receipts");
        KiiQuery query = new KiiQuery(KiiClause.equals("product_id", getId()));
        query.setLimit(20);
        KiiQueryResult<KiiObject> result = bucket.query(query);
        if (result != null && result.getResult() != null && result.getResult().size() > 0) {
            return true;
        } else {
            HttpPost request = new HttpPost(GET_ORDER_URL);
            JSONObject object = new JSONObject();
            object.put("app_id", Kii.getAppId());
            object.put("app_key", Kii.getAppKey());
            object.put("product_id", getId());
            request.setEntity(new StringEntity(object.toString(), "UTF-8"));
            HttpResponse response = new DefaultHttpClient().execute(request);
            String ret = EntityUtils.toString(response.getEntity());
            JSONObject orders = new JSONObject(ret);
            JSONArray array = orders.getJSONArray("results");
            for (int i = 0; i < array.length(); i++) {
                JSONObject order = array.getJSONObject(i);
                if (order.optString("pay_status", "").contentEquals("paid")) {
                    return true;
                }
            }
        }
        return false;
    }

}
