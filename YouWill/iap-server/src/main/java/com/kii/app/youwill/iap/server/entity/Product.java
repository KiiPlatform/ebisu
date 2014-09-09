package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

/**
 * Created by ethan on 14-7-25.
 */
public class Product extends KiiEntity {

    /*
    productname
description
category
is_new
is_recommended
price
valid
consume_type
currency
thumbnail
orther customer fields in key:value pair
     */


    public Product(JSONObject json) {
        super(json);

        try {
            this.productID = json.getString("productID");
            this.productName = json.getString("name");
            this.description = json.getString("description");
            this.price = json.getString("price");
            if (json.has("valid")) {
                this.valid = json.getBoolean("valid");
            }
            this.consumeType = ConsumeType.valueOf(json.getString("consumeType"));

        } catch (JSONException e) {
            throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
        }
    }

    private String productID;

    private String productName;

    private String description;

    private String price;

    private boolean valid;

    private ConsumeType consumeType;

    private CurrencyType currency;


    public String getPrice() {
        return price;
    }

    public boolean isValid() {
        return valid;
    }

    public ConsumeType getConsumeType() {
        return consumeType;
    }

    public CurrencyType getCurrency() {
        return currency;
    }


    public String getProductID() {
        return productID;
    }

    public String getProductName() {
        return productName;
    }

    public String getDescription() {
        return description;
    }
}
