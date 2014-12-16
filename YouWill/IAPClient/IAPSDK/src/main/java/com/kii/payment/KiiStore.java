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
 * This is a utility class to access KiiProduct, and KiiReceipt from KiiCloud.
 *
 * Created by tian on 2/27/14.
 */
public class KiiStore {

    private static final String TAG = KiiStore.class.getName();

    public static KiiProduct getProductByID(String id) {
        KiiClause clause = KiiClause.equals("_id", id);
        KiiQuery localQuery = new KiiQuery(clause);
        KiiProduct product = null;
        try {
            KiiQueryResult<KiiObject> result = Kii.bucket("product").query(localQuery);

            if (result.getResult().size() > 0) {
                product = new KiiProduct(result.getResult().get(0));
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return product;
    }

    /**
     * List KiiProduct controlled by the specified KiiClause.
     *
     * @param clause
     * @return
     */
    public static List<KiiProduct> listProducts(KiiClause clause) {
        KiiClause appFilterClause = KiiClause.equals("appID", YouWillIAPSDK.getYouWillAppID());

        KiiQuery localQuery = new KiiQuery(
                clause == null ? appFilterClause : clause);
        try {
            KiiQueryResult<KiiObject> result = Kii.bucket("product").query(localQuery);
            List<KiiProduct> products = new ArrayList<KiiProduct>();
            for (KiiObject object : result.getResult()) {
                //Utils.log(TAG, "listProducts, get object is " + object.toJSON());
                products.add(new KiiProduct(object));
            }
            return products;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * List KiiReceipt of the KiiUser, controlled by the specified KiiClause.
     *
     * @param clause
     * @param user
     * @return
     */
    public static List<KiiReceipt> listReceipts(KiiClause clause, KiiUser user) {
        KiiClause appFilterClause = KiiClause.equals("appID", YouWillIAPSDK.getYouWillAppID());
        KiiQuery localQuery = new KiiQuery(
                clause == null ? appFilterClause : clause);
        localQuery.sortByDesc("_created");
        try {
            KiiQueryResult<KiiObject> result = user.bucket("receipt").query(localQuery);
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

    /**
     * Get a KiiReceipt of the KiiUser for one particular KiiProduct, specified by the productID.
     * @param productID
     * @param user
     * @return
     */
    public static KiiReceipt getReceipt(String productID, KiiUser user) {
        List<KiiReceipt> receipts = listReceipts(KiiClause.equals("productID", productID),
                user);
        return (receipts != null && receipts.size() > 0) ? receipts.get(0) : null;
    }


    private static String toHexString(byte[] b) {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (int i = 0; i < b.length; i++) {
            sb.append(HEX_DIGITS[(b[i] & 0xf0) >>> 4]);
            sb.append(HEX_DIGITS[b[i] & 0x0f]);
        }
        return sb.toString();
    }

    private static final char HEX_DIGITS[] = {'0', '1', '2', '3', '4', '5',
            '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    private static String buildHash(String params, String key) {
        try {
            //Utils.log(TAG, "buildHash, params is " + params);
            //Utils.log(TAG, "buildHash, key is " + key);
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
