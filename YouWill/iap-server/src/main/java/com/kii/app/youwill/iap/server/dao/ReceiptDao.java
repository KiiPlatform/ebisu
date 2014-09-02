package com.kii.app.youwill.iap.server.dao;

import com.kii.app.youwill.iap.server.entity.Receipt;
import com.kii.platform.ufp.bucket.ObjectID;

/**
 * Created by ethan on 14-8-7.
 */
public interface ReceiptDao {

	ObjectID createNewReceipt(Receipt receipt);

	boolean existProduct(String id);


}
