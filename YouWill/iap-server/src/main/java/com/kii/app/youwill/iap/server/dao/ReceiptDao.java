package com.kii.app.youwill.iap.server.dao;

import com.kii.app.youwill.iap.server.entity.Receipt;
import com.kii.platform.ufp.bucket.ObjectID;
import com.kii.platform.ufp.user.UserID;

import java.util.Map;

/**
 * Created by ethan on 14-8-7.
 */
public interface ReceiptDao {

	ObjectID createNewReceipt(Receipt receipt,UserID userID);

	boolean existProduct(String productID, UserID userID);


}
