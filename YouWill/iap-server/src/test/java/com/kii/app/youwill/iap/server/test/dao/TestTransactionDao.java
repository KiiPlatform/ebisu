package com.kii.app.youwill.iap.server.test.dao;

import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.entity.*;
import com.kii.app.youwill.iap.server.service.StartTransactionParam;
import com.kii.app.youwill.iap.server.test.BaseTest;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.*;
import com.kii.platform.ufp.ufe.ObjectCreationResponse;
import com.kii.platform.ufp.ufe.QueryRequest;
import com.kii.platform.ufp.ufe.QueryResponse;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.errors.BucketNotFoundException;
import com.kii.platform.ufp.ufe.query.clauses.AndClause;
import com.kii.platform.ufp.ufe.query.clauses.Clause;
import com.kii.platform.ufp.ufe.query.clauses.EqualsClause;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.Test;
import org.mockito.ArgumentMatcher;
import org.springframework.beans.factory.annotation.Autowired;

import java.util.Date;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;

/**
 * Created by ethan on 14-8-20.
 */
public class TestTransactionDao extends BaseTest {


	private String BUCKET="transaction";

	@Autowired
	private BucketClient bucketClient;

	@Autowired
	private AppContext appContext;

	@Autowired
	private TransactionDao transDao;

	String transID;


	Date date=new Date();


	class TestObj extends ArgumentMatcher<JSONObject> {

		String token;

		boolean sign=false;

		public TestObj(String token){
			this.token=token;
		}


		public TestObj(String token,boolean sign){
			this.token=token;
			this.sign=sign;
		}

		@Override
		public boolean matches(Object argument) {

			JSONObject json=(JSONObject)argument;

			try {
				assertEquals(json.getString("price"), "34.12");

				assertEquals(json.getString("userID"), token);
				assertEquals(json.getString("payStatus"), OrderStatus.pending.name());

				assertEquals(json.getString("payType"), PayType.alipay.name());

				assertEquals(json.getString("currency"), CurrencyType.CNY.name());
				assertEquals(json.getString("productID"), "prod1");

				if(sign) {
					assertEquals(json.getBoolean("isSandbox"), true);
				}
				transID=json.getString("transactionID");

				return true;
			}catch(Exception e){
				e.printStackTrace();
				return false;
			}
		}
	}

	 class QueryReq extends ArgumentMatcher<QueryRequest> {


		 private boolean sign;

		 public QueryReq(){
			 sign=false;
		 }

		 public QueryReq(boolean sign){
			 this.sign=sign;
		 }

		@Override
		public boolean matches(Object argument) {

			QueryRequest req=(QueryRequest)argument;

			Clause clause=req.getBucketQuery().getClause();

			if(sign){

				assertTrue((clause instanceof AndClause));

				AndClause andClu = (AndClause) clause;

				Clause[] claus=andClu.getClauses();

				for(Clause clau:claus){
					EqualsClause equ=(EqualsClause)clau;
					if(equ.getField().equals("transactionID")){
						assertEquals(equ.getValue(), transID);

					}else if(equ.getField().equals("isSandbox")){
						assertEquals(equ.getValue(), true);

					}

				}


			}else {
				assertTrue((clause instanceof EqualsClause));
				EqualsClause equ = (EqualsClause) clause;

				assertEquals(equ.getField(), "transactionID");

				assertEquals(equ.getValue(), transID);
			}

			return true;
		}
	}

	private QueryResponse getQueryResponse() throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();
		JSONObject trans=getEntity();

		trans.put("productID","prod1");
		trans.put("price","34.12");
		trans.put("userID",token);
		trans.put("payStatus", OrderStatus.pending);
		trans.put("createdDate",new Date().getTime());
		trans.put("transactionID","transID");
		trans.put("currency", CurrencyType.CNY);

		objList.put(trans);
		resp.setResults(objList);
		return resp;
	}






	@Test
	public void testAddTransaction() throws JSONException, BucketNotFoundException {

		TestObj  jsonReq=new TestObj(token);


		super.initRequest();


		//appContext.asApp();


		ObjectCreationResponse resp=new ObjectCreationResponse();
		resp.setObjectID(new ObjectID("newID"));


		when(bucketClient.createDataObject(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				argThat(jsonReq))).thenReturn(resp);


		QueryReq queryReq=new QueryReq();

		when(bucketClient.query(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(queryReq))).thenReturn(getQueryResponse());


		JSONObject jsonPro=getEntity();

		jsonPro.put("productID","prod1");
		jsonPro.put("price", "12.34");
		jsonPro.put("consumeType", ConsumeType.periodical);

		Product product=new Product(jsonPro);

		JSONObject jsonParam=new JSONObject();
		jsonParam.put("payType", PayType.alipay);
		jsonParam.put("verifySign","verifySign");
		jsonParam.put("price","34.12");

		assertEquals(product.getProductID(),"prod1");

		StartTransactionParam param=new StartTransactionParam(jsonParam.toString());

		String id=transDao.createNewOrder(product,param);


		Transaction  trans=transDao.getOrderByTransactionID(id);
		assertEquals(trans.getTransactionID(),"transID");

	}


	@Test
	public void testAddTransactionSandBox() throws JSONException, BucketNotFoundException {



		super.initRequestWithSandbox();
		assertEquals(appContext.isSandBox(), true);


		//appContext.asApp();


		ObjectCreationResponse resp=new ObjectCreationResponse();
		resp.setObjectID(new ObjectID("newID"));


		TestObj  jsonReq=new TestObj(token,true);

		when(bucketClient.createDataObject(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				argThat(jsonReq))).thenReturn(resp);


		QueryReq queryReq=new QueryReq(true);

		when(bucketClient.query(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(queryReq))).thenReturn(getQueryResponse());


		JSONObject jsonPro=getEntity();

		jsonPro.put("productID","prod1");
		jsonPro.put("price", "12.34");
		jsonPro.put("consumeType", ConsumeType.periodical);

		Product product=new Product(jsonPro);

		JSONObject jsonParam=new JSONObject();
		jsonParam.put("payType",PayType.alipay);
		jsonParam.put("verifySign","verifySign");
		jsonParam.put("price","34.12");

		assertEquals(product.getProductID(),"prod1");

		StartTransactionParam param=new StartTransactionParam(jsonParam.toString());

		String id=transDao.createNewOrder(product,param);

		Transaction  trans=transDao.getOrderByTransactionID(id);
		assertEquals(trans.getTransactionID(),"transID");


	}






}
