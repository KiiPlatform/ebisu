package com.kii.app.youwill.iap.server.test.dao;

import com.kii.app.youwill.iap.server.test.BaseTest;
import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.BucketID;
import com.kii.platform.ufp.bucket.BucketType;
import com.kii.platform.ufp.bucket.PaginationKey;
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;

/**
 * Created by ethan on 14-8-21.
 */
public class TestReceiptDao extends BaseTest {


	private String BUCKET="receipt";

	@Autowired
	private BucketClient bucketClient;

	@Autowired
	private AppContext appContext;

	@Autowired
	private ReceiptDao receiptDao;

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
					if(equ.getField().equals("productID")){
						assertEquals(equ.getValue(), "prod1");

					}else if(equ.getField().equals("isSandbox")){
						assertEquals(equ.getValue(), true);

					}

				}


			}else {
				assertTrue((clause instanceof EqualsClause));
				EqualsClause equ = (EqualsClause) clause;

				assertEquals(equ.getField(), "productID");

				assertEquals(equ.getValue(), "prod1");
			}

			return true;
		}
	}

	private QueryResponse getQueryResponse(int idx) throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();
		for(int i=0;i<idx;i++) {
			JSONObject trans = getEntity();
			objList.put(trans);
		}
		resp.setResults(objList);
		return resp;
	}

	@Test
	public void testCreateReceipt() throws BucketNotFoundException, JSONException {



		super.initRequest();


		//appContext.asApp();

		QueryReq  jsonReq=new QueryReq(appContext.isSandBox());

		when(bucketClient.query(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(jsonReq))).thenReturn(getQueryResponse(1));


		//assertTrue(receiptDao.existProduct("prod1"));


		QueryResponse resp=new QueryResponse();
		resp.setNextPaginationKey(new PaginationKey(""));

		when(bucketClient.query(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(jsonReq))).thenReturn(getQueryResponse(0));

		//assertFalse(receiptDao.existProduct("prod1"));


	}

	@Test
	public void testCreateReceiptSandBox() throws BucketNotFoundException, JSONException {



		super.initRequestWithSandbox();


		//appContext.asApp();

		QueryReq  jsonReq=new QueryReq(appContext.isSandBox());

		when(bucketClient.query(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(jsonReq))).thenReturn(getQueryResponse(1));


		//assertTrue(receiptDao.existProduct("prod1"));


		QueryResponse resp=new QueryResponse();
		resp.setNextPaginationKey(new PaginationKey(""));

		when(bucketClient.query(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(jsonReq))).thenReturn(getQueryResponse(0));

		//assertFalse(receiptDao.existProduct("prod1"));


	}
}
