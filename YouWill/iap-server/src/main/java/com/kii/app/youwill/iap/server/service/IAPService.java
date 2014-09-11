package com.kii.app.youwill.iap.server.service;

import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.dao.ProductDao;
import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.dao.impl.AlipayQueryAccessHelper;
import com.kii.app.youwill.iap.server.dao.impl.PaypalQueryAccessHelper;
import com.kii.app.youwill.iap.server.entity.*;
import com.kii.app.youwill.iap.server.web.AppContext;
import org.apache.commons.lang3.StringUtils;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

/**
 * Created by ethan on 14-7-28.
 */
@Component
public class IAPService {
	@Autowired
	private AppContext context;
	@Autowired
	private ProductDao productDao;

	@Autowired
	private ConfigInfoStore configStore;

	@Autowired
	private ReceiptDao receiptDao;

	@Autowired
	private TransactionDao transactionDao;

	@Autowired
	private AlipayQueryAccessHelper alipayAccess;
	@Autowired
	private PaypalQueryAccessHelper paypalAccess;



	public JSONObject startOrderTransaction(StartTransactionParam param){
		/*
		TODO: disable validation
		if(!param.valid(context.getAppID().toString(), configStore.getIAPSecurityKey())){
			throw new ServiceException(IAPErrorCode.SIGN_INVALID);
		}*/

		Product product=productDao.getProductByID(param.getProductID());

		if(product==null){
			throw new ServiceException(IAPErrorCode.PRODUCT_NOT_FOUND);
		}

		if(StringUtils.isBlank(param.getPrice())){
			param.setPrice(String.valueOf(product.getPrice()));
		}


		if(product.getConsumeType()== ConsumeType.consumable
				&&receiptDao.existProduct(product.getId())){

			throw new ServiceException(IAPErrorCode.PRODUCT_BOUGHT_ALREADY);

		}

		String data =transactionDao.createNewOrder(product,param);
		JSONObject jsObj=new JSONObject();
		try {
			jsObj.put("payType",param.getPayType().name());

			if(param.getPayType()== PayType.alipay){
                jsObj.put("url", data);
			}else if(param.getPayType()==PayType.paypal){
                jsObj.put("transactionID",data);
				jsObj.put("payInfo",this.configStore.getPaypalConfig().getPaypayJson());

			}
			return jsObj;

		} catch (JSONException e) {
			throw new IllegalArgumentException(e);
		}

	}

	public String finishOrder(FinishTransactionParam param ) {


		if (!param.valid(context.getAppID().toString(), configStore.getIAPSecurityKey())) {
			throw new ServiceException(IAPErrorCode.SIGN_INVALID);
		}

		Transaction transaction = transactionDao.getOrderByTransactionID(param.getTransactionID());

		if (transaction == null) {
			throw new ServiceException(IAPErrorCode.ORDER_NOT_FOUND);
		}


		switch (transaction.getPayType()) {

			case alipay:
                /*
				AlipayQueryResult alipayResult = alipayAccess.queryTransactionStatus(transaction.getTransactionID());

				OrderStatus status = transactionDao.completeAlipayPay(transaction, alipayResult);

				if (transaction.getPayStatus() == OrderStatus.pending &&
						(status == OrderStatus.completed || status == OrderStatus.success)) {
					receiptDao.createNewReceipt(new Receipt(transaction, alipayResult));
				}
                */
				return OrderStatus.completed.toString();

			case paypal:

				if (StringUtils.isBlank(param.getPaymentID())) {
					throw new ServiceException(IAPErrorCode.PAYID_NOT_FOUND);
				}

				PaypalQueryResult result = paypalAccess.doPaypalQuery(param.getPaymentID());

				OrderStatus paypalStatus = transactionDao.completePaypalPay(transaction, param.getPaymentID(), result);

				if (transaction.getPayStatus() == OrderStatus.pending &&
						(paypalStatus == OrderStatus.completed || paypalStatus == OrderStatus.success)) {
					receiptDao.createNewReceipt(new Receipt(transaction, result), null);
				}
				return paypalStatus.name();

			default:
				return OrderStatus.success.name();

		}

	}
}
