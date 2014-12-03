package com.kii.app.youwill.iap.server.dao;

import com.kii.app.youwill.iap.server.entity.*;
import com.kii.app.youwill.iap.server.service.StartTransactionParam;
import net.sf.json.JSONObject;

import java.util.Map;

/**
 * Created by ethan on 14-8-7.
 */
public interface TransactionDao {
    String createNewOrder(Product product, StartTransactionParam param);

    Transaction getOrderByTransactionID(String transactionID);

    OrderStatus completeAlipayPay(Transaction trans, Map<String, String> callbackParams);

    OrderStatus completePaypalPay(Transaction trans, String paymentID, PaypalQueryResult result);


    OrderStatus completeMMPay(Transaction transaction, JSONObject jsonObject);
}
