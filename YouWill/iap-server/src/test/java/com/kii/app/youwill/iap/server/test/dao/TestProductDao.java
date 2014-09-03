package com.kii.app.youwill.iap.server.test.dao;

import com.kii.app.youwill.iap.server.test.BaseTest;
import com.kii.app.youwill.iap.server.dao.ProductDao;
import com.kii.app.youwill.iap.server.entity.ConsumeType;
import com.kii.app.youwill.iap.server.entity.Product;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.BucketID;
import com.kii.platform.ufp.bucket.BucketType;
import com.kii.platform.ufp.bucket.PaginationKey;
import com.kii.platform.ufp.ufe.QueryRequest;
import com.kii.platform.ufp.ufe.QueryResponse;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.errors.BucketNotFoundException;
import com.kii.platform.ufp.ufe.query.BucketQuery;
import com.kii.platform.ufp.ufe.query.clauses.Clause;
import com.kii.platform.ufp.ufe.query.clauses.EqualsClause;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentMatcher;
import org.springframework.beans.factory.annotation.Autowired;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;

/**
 * Created by ethan on 14-8-20.
 */
public class TestProductDao extends BaseTest {


	@Autowired
	private AppContext appContext;

	@Autowired
	private ProductDao productDao;

	@Autowired
	private BucketClient bucketClient;

	private String BUCKET="product";

	static class Resp extends ArgumentMatcher<QueryRequest> {

		@Override
		public boolean matches(Object argument) {

			QueryRequest req=(QueryRequest)argument;

			Clause clause=req.getBucketQuery().getClause();
			assertTrue((clause instanceof EqualsClause));
			EqualsClause  equ=(EqualsClause)clause;

			assertEquals(equ.getField(), "productID");

			assertEquals(equ.getValue(), "prod1");


			return true;
		}
	}

	@Before
	public void init() throws BucketNotFoundException, JSONException {

		super.initRequest();

		appContext.asApp();

		when(bucketClient.query(
				eq(appContext.getAccessToken()),
				eq(appContext.getCurrScope()),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(new Resp()))).thenReturn(getQueryResponse("prod1", "12.34"));

		appContext.exitApp();

	}

	private QueryRequest getQueryRequest(String prodID) {
		BucketQuery query=new BucketQuery(BucketQuery.qEquals("productID",prodID));

		return new QueryRequest(query);
	}

	private QueryResponse getQueryResponse(String prodID,String price) throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();
		JSONObject prod=getEntity();
		prod.put("productID",prodID);
		prod.put("price",price);
		prod.put("consumeType", ConsumeType.periodical);


		objList.put(prod);
		resp.setResults(objList);
		return resp;
	}



	@Test
	public void testSeleProduct() throws BucketNotFoundException {



		Product prod1=productDao.getProductByID("prod1");

		assertEquals(prod1.getProductID(),"prod1");
		assertEquals(prod1.getPrice(),"12.34");
	}

}
