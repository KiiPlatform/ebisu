package com.kii.app.youwill.iap.server.test.dao;

import com.kii.app.youwill.iap.server.dao.CommBucketOperate;
import com.kii.app.youwill.iap.server.test.BaseTest;
import com.kii.platform.ufp.app.AppID;
import com.kii.platform.ufp.bucket.*;
import com.kii.platform.ufp.errors.UserNotFoundException;
import com.kii.platform.ufp.ufe.ObjectCreationResponse;
import com.kii.platform.ufp.ufe.QueryRequest;
import com.kii.platform.ufp.ufe.QueryResponse;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.errors.BucketNotFoundException;
import com.kii.platform.ufp.ufe.query.BucketQuery;
import com.kii.platform.ufp.ufe.query.clauses.Clause;
import com.kii.platform.ufp.ufe.query.clauses.EqualsClause;
import com.kii.platform.ufp.user.UserID;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentMatcher;
import org.springframework.beans.factory.annotation.Autowired;

import static junit.framework.TestCase.assertTrue;
import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;
import static org.springframework.test.util.MatcherAssertionErrors.assertThat;

/**
 * Created by ethan on 14-8-19.
 */
//@RunWith(MockitoJUnitRunner.class)
public class TestCommBucketDao extends BaseTest {




	@Autowired
	private CommBucketOperate commOper;

	@Autowired
	private BucketClient bucketClient;


	String bucketName="mockBucket";



	@Before
	public void init() throws UserNotFoundException {


		super.initRequest();

	}


	@Test
	public void testQuery() throws BucketNotFoundException, JSONException {


		BucketQuery query=getQueryRequest("prod1");



		when(bucketClient.query(
				eq(appContext.getAccessToken()),
//				eq(appContext.getCurrScope()),
				argThat(new ArgumentMatcher<ObjectScope>() {
					@Override
					public boolean matches(Object argument) {
						ObjectScope scope=(ObjectScope)argument;

						assertEquals(appContext.getCurrScope().getAppID(),scope.getAppID());
						assertEquals(appContext.getCurrScope().getUserID(),scope.getUserID());

						return true;
					}
				}),
				eq(new BucketID(bucketName)),
				eq(BucketType.DEFAULT),
				argThat(new ArgumentMatcher<QueryRequest>(){

					@Override
					public boolean matches(Object argument) {
						QueryRequest request=(QueryRequest)argument;

						BucketQuery query=request.getBucketQuery();
						Clause clause=query.getClause();

						assertTrue(clause instanceof EqualsClause);
						assertEquals(((EqualsClause) clause).getField(),"productID");
						assertEquals(((EqualsClause) clause).getValue(),"prod1");

						return true;
					}
				})
				))
				.thenReturn(getQueryResponse("prod1", "12.34"));



		CommBucketOperate.QueryResult result=commOper.query(bucketName,query);




		assertEquals(1, result.getResultList().size());


	}



	private BucketQuery getQueryRequest(String prodID) {
		BucketQuery query=new BucketQuery(BucketQuery.qEquals("productID",prodID));

		return query;
	}

	private QueryResponse getQueryResponse(String prodID,String price) throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();
		JSONObject prod=new JSONObject();
		prod.put("productID",prodID);
		prod.put("price",price);

		objList.put(prod);
		resp.setResults(objList);
		return resp;
	}

	@Test
	public void testAddObj() throws JSONException, BucketNotFoundException {


		JSONObject obj=new JSONObject();
		obj.put("foo","bar");

		ObjectCreationResponse resp=new ObjectCreationResponse();
		resp.setObjectID(new ObjectID("fooID"));


		when(bucketClient.createDataObject(
				eq(appContext.getAccessToken()),
				eq(new ObjectScope(new AppID(appID),new UserID(token))),
				eq(new BucketID(bucketName)),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				eq(obj)
		)).thenReturn(resp);

		ObjectID id=commOper.addObject(bucketName, obj);

		assertThat(id.toString(), is("fooID"));

		//switch object scope.
		appContext.asApp();


		when(bucketClient.createDataObject(
				eq(appContext.getAccessToken()),
				eq(new ObjectScope(new AppID(appID))),
				eq(new BucketID(bucketName)),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				eq(obj)
		)).thenReturn(resp);

		 id=commOper.addObject(bucketName, obj);

		assertThat(id.toString(),is("fooID"));



	}


}
