package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.aop.KiiScope;
import com.kii.app.youwill.iap.server.aop.ScopeType;
import com.kii.app.youwill.iap.server.dao.*;
import com.kii.app.youwill.iap.server.entity.*;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.app.youwill.iap.server.service.StartTransactionParam;
import com.kii.app.youwill.iap.server.service.UUIDGeneral;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.ObjectID;
import com.kii.platform.ufp.ufe.query.BucketQuery;
import org.apache.commons.lang3.StringUtils;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Created by ethan on 14-7-25.
 */
@Component
@KiiScope(scope= ScopeType.Admin)
public class TransactionDaoImpl implements TransactionDao {

	@Autowired
	private CommBucketOperate commDao;
	@Autowired
	private AppContext appContext;
	@Autowired
	private UUIDGeneral uuidGeneral;
	@Autowired
	private ACLOperate aclOper;
//	@Autowired
//	private AppConfigureOperate configOper;
	@Autowired
	private ConfigInfoStore store;


	private String BUCKET_ID="transaction";


	@Override public String createNewOrder(Product product,StartTransactionParam param) {

		String transactionID=param.getTransactionID();
		if(StringUtils.isBlank(transactionID)) {

			transactionID = uuidGeneral.getUUID(appContext.getCurrUserID().toString());
		}

		Transaction transaction=new Transaction(product,
				appContext.getCurrUserID(),
				CurrencyType.CNY);
		transaction.setTransactionID(transactionID);
		transaction.setPayType(param.getPayType());
		transaction.setPrice(param.getPrice());
		if(appContext.isSandBox()) {
			transaction.setSandBox(appContext.isSandBox());
		}

		ObjectID id=commDao.addObject(BUCKET_ID, transaction.getJsonObject());

		aclOper.removeObjectACLForSpecUser(BUCKET_ID, id, ACLOperate.ObjectRight.Write, ACLOperate.SpecUser.ANY_AUTHENTICATED_USER);

		aclOper.removeObjectACLForSpecUser(BUCKET_ID,id, ACLOperate.ObjectRight.Read ,ACLOperate.SpecUser.ANY_AUTHENTICATED_USER);

		return transactionID;
	}


	@Override public Transaction getOrderByTransactionID(String transactionID) {


		BucketQuery query=null;


		if(appContext.isSandBox()){
			query =new BucketQuery(BucketQuery.qAnd(
					BucketQuery.qEquals("transactionID",transactionID),
					BucketQuery.qEquals("isSandbox",appContext.isSandBox())));

		}else{
			query =new BucketQuery(BucketQuery.qEquals("transactionID",transactionID));
		}


		CommBucketOperate.QueryResult result=commDao.query(BUCKET_ID,query);

		List<JSONObject> list=result.getResultList();
		if(list.isEmpty()){
			throw new ServiceException(IAPErrorCode.ORDER_NOT_FOUND);
		}

		JSONObject obj=list.get(0);

		return new Transaction(obj);
	}




	@Override
	public OrderStatus completeAlipayPay(Transaction transaction,AlipayQueryResult result) {




		if(transaction.getPayStatus()== OrderStatus.pending||
				(transaction.getPayStatus()==OrderStatus.success&&result.getTradeStatus()==TradeStatus.TRADE_FINISHED)){

			Map<String,Object> map=new HashMap<String,Object>();
			OrderStatus status=null;

			switch(result.getTradeStatus()){
				case TRADE_SUCCESS:
					status=OrderStatus.success;
					break;
				case TRADE_FINISHED:
					status=OrderStatus.completed;
					break;
				case TRADE_PENDING:
					status=OrderStatus.pending;
					break;
				case TRADE_CLOSED:
					status=OrderStatus.error;
					break;
				case TRADE_REFUSE:
				case TRADE_REFUSE_DEALING:
				case TRADE_CANCEL:
					status=OrderStatus.cancel;
					break;
			}
			if(status==OrderStatus.pending){
				return status;
			}

			map.put("payStatus", status.name());


			map.put("payCompleteDate",result.getGmtPayment());

			map.put("alipayTradeNo",result.getTradeID());
			map.put("modifyDate",new Date());

			commDao.updateParticObjWithVer(BUCKET_ID,transaction.getId(),transaction.getVersion(),map);

			return status;

		}else{
			throw new ServiceException(IAPErrorCode.PAY_STATUS_INVALID);
		}

	}

	@Override
	public OrderStatus completePaypalPay(Transaction transaction, String paymentID,PaypalQueryResult result) {



		if(transaction.getPayStatus()!= OrderStatus.pending) {
			throw new ServiceException(IAPErrorCode.PAY_STATUS_INVALID);
		}


			Map<String,Object> map=new HashMap<String,Object>();
			OrderStatus status=null;

			switch(result.getState()){
				case approved:
					status=OrderStatus.completed;
					break;
				case failed:
				case expired:
					status=OrderStatus.error;
					break;
				case canceled:
					status=OrderStatus.cancel;
					break;
				case created:
					status=OrderStatus.pending;
			}
			if(status==OrderStatus.pending){
				return status;
			}

			map.put("payStatus", status.name());

			map.put("payCompleteDate",result.getUpdateTime());

			map.put("paypalPaymentID",result.getId());
			map.put("modifyDate",new Date());

			commDao.updateParticObjWithVer(BUCKET_ID,transaction.getId(),transaction.getVersion(),map);

			return status;

	}


}
