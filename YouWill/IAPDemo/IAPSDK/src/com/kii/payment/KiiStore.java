package com.kii.payment;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.query.KiiClause;
import com.kii.cloud.storage.query.KiiQuery;
import com.kii.cloud.storage.query.KiiQueryResult;

import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by tian on 2/27/14.
 */
public class KiiStore {

    private static final String TAG = KiiStore.class.getName();

    public static List<KiiProduct> listProducts(KiiQuery query) {
        KiiClause clause = KiiClause.equals("appId", YouWillIAPSDK.gYouWillAppId);
        KiiQuery localQuery = query;
        if (localQuery == null) {
            localQuery = new KiiQuery(clause);
        }
        try {
            KiiQueryResult<KiiObject> result = Kii.bucket("products").query(localQuery);
            List<KiiProduct> products = new ArrayList<KiiProduct>();
            for (KiiObject object : result.getResult()) {
                Utils.log(TAG, "listProducts, get object is " + object.toJSON());
                products.add(new KiiProduct(object));
            }
            return products;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static List<KiiReceipt> listReceipts(KiiQuery query, KiiUser user) {
        KiiQuery localQuery = query;
        KiiClause clause = KiiClause.equals("appId", YouWillIAPSDK.gYouWillAppId);
        if (localQuery == null) {
            localQuery = new KiiQuery(clause);
        }
        try {
            KiiQueryResult<KiiObject> result = user.bucket("receipts").query(localQuery);
            List<KiiReceipt> receipts = new ArrayList<KiiReceipt>();
            for (KiiObject object : result.getResult()) {
                receipts.add(new KiiReceipt(object));
            }
            return receipts;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static KiiReceipt getReceipt(KiiProduct product, KiiUser user, String appId) {
        KiiQuery query = new KiiQuery(KiiClause.equals("product_id", product.getId()));
        List<KiiReceipt> receipts = listReceipts(query, user);
        return (receipts != null && receipts.size() > 0) ? receipts.get(0) : null;
    }


    public static String toHexString(byte[] b) {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (int i = 0; i < b.length; i++) {
            sb.append(HEX_DIGITS[(b[i] & 0xf0) >>> 4]);
            sb.append(HEX_DIGITS[b[i] & 0x0f]);
        }
        return sb.toString();
    }

    private static final char HEX_DIGITS[] = {'0', '1', '2', '3', '4', '5',
            '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    public static String buildHash(String params, String key) {
        try {
            Utils.log(TAG, "buildHash, params is " + params);
            Utils.log(TAG, "buildHash, key is " + key);
            String data = params + key;
            MessageDigest md5 = MessageDigest.getInstance("MD5");
            md5.update(data.getBytes("UTF-8"));
            return toHexString(md5.digest());
        } catch (NoSuchAlgorithmException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
        return null;

    }
}
