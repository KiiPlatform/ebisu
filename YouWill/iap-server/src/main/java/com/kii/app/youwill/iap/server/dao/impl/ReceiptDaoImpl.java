package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.aop.KiiScope;
import com.kii.app.youwill.iap.server.dao.ACLOperate;
import com.kii.app.youwill.iap.server.dao.CommBucketOperate;
import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.entity.Receipt;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.ObjectID;
import com.kii.platform.ufp.ufe.query.BucketQuery;
import com.kii.platform.ufp.ufe.query.clauses.EqualsClause;
import com.kii.platform.ufp.user.UserID;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

import java.util.Map;

/**
 * Created by ethan on 14-7-25.
 */
@Component
@KiiScope
public class ReceiptDaoImpl implements ReceiptDao {

	private static final String BUCKET_NAME = "receipt";
	@Autowired
	private CommBucketOperate commDao;

	@Autowired
	private AppContext appContext;

	@Autowired
	private ACLOperate aclOper;

	@Override
    public ObjectID createNewReceipt(Receipt receipt,UserID userID) {

        System.out.println("UserID:" + userID.toString());
        appContext.sudo(userID);

		ObjectID id=commDao.addObject(BUCKET_NAME, receipt.getJsonObject());

//		aclOper.removeObjectACLForCurrUser(BUCKET_NAME, id, ACLOperate.ObjectRight.Write);

        appContext.exitScope();
		return id;

	}

	@Override public boolean existProduct(String id) {

		BucketQuery query=null;

		if(appContext.isSandBox()) {
		    query = new BucketQuery(BucketQuery.qAnd(
					new EqualsClause("productID", id),
					new EqualsClause("isSandbox", appContext.isSandBox())));
		}else{
			query = new BucketQuery(BucketQuery.qEquals("productID",id));

		}
		CommBucketOperate.QueryResult result=commDao.query(BUCKET_NAME,query);

		return !result.getResultList().isEmpty();

	}
}
