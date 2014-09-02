package com.kii.app.youwill.iap.server.dao;

import com.kii.app.youwill.iap.server.entity.*;
import com.kii.app.youwill.iap.server.service.StartTransactionParam;

/**
 * Created by ethan on 14-8-7.
 */
public interface TransactionDao {
	String createNewOrder(Product product, StartTransactionParam param);

	Transaction getOrderByTransactionID(String transactionID);

	OrderStatus completeAlipayPay(Transaction trans,AlipayQueryResult result);

	OrderStatus completePaypalPay(Transaction trans,String paymentID,PaypalQueryResult result);



}
