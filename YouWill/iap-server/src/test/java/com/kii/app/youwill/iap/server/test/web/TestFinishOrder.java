package com.kii.app.youwill.iap.server.test.web;

import com.kii.app.youwill.iap.server.dao.impl.AlipayQueryAccessHelper;
import com.kii.app.youwill.iap.server.entity.AlipayQueryResult;
import com.kii.app.youwill.iap.server.entity.CurrencyType;
import com.kii.app.youwill.iap.server.entity.OrderStatus;
import com.kii.app.youwill.iap.server.entity.PayType;
import com.kii.platform.ufp.bucket.*;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.pagination.PaginationKey;
import com.kii.platform.ufp.scope.ObjectScope;
import com.kii.platform.ufp.ufe.ObjectCreationResponse;
import com.kii.platform.ufp.ufe.QueryRequest;
import com.kii.platform.ufp.ufe.QueryResponse;
import com.kii.platform.ufp.ufe.query.clauses.AndClause;
import com.kii.platform.ufp.ufe.query.clauses.Clause;
import com.kii.platform.ufp.ufe.query.clauses.EqualsClause;
import org.apache.commons.codec.digest.DigestUtils;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.Assert;
import org.junit.Test;
import org.mockito.ArgumentMatcher;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.core.io.ResourceLoader;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MvcResult;
import org.springframework.test.web.servlet.ResultMatcher;

import java.io.IOException;
import java.io.InputStream;
import java.util.Date;

import static junit.framework.TestCase.assertTrue;
import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.*;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.content;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;


public class TestFinishOrder extends BaseWebTest {


	@Autowired
	private AlipayQueryAccessHelper  accessHelper;


	@Autowired
	private ResourceLoader loader;




	private String appID="mockApp";
	private String appKey="mockKey";
	private String token="mockToken";

	private String price="12.34";
	private String tranID="tran1";


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

				Assert.assertTrue((clause instanceof AndClause));

				AndClause andClu = (AndClause) clause;

				Clause[] claus=andClu.getClauses();

				for(Clause clau:claus){
					EqualsClause equ=(EqualsClause)clau;
					if(equ.getField().equals("transactionID")){
						assertEquals(equ.getValue(), tranID);

					}else if(equ.getField().equals("isSandbox")){
						assertEquals(equ.getValue(), true);

					}

				}


			}else {
				Assert.assertTrue((clause instanceof EqualsClause));
				EqualsClause equ = (EqualsClause) clause;

				assertEquals(equ.getField(), "transactionID");

				assertEquals(equ.getValue(), tranID);
			}

			return true;
		}
	}

	private class UpdateReq extends ArgumentMatcher<JSONObject>  {


		@Override
		public boolean matches(Object argument) {

			assertTrue(argument instanceof JSONObject);

			JSONObject obj=(JSONObject)argument;

			try {

				assertEquals(obj.getString("payStatus"), OrderStatus.completed.name());


			}catch(Exception e){
				e.printStackTrace();
			}
			return true;
		}


	}


	private QueryResponse getTransactionResponse() throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();

		JSONObject prod = getTransaction();

		objList.put(prod);
		resp.setResults(objList);
		return resp;
	}

	private JSONObject getTransaction() throws JSONException {
		JSONObject prod= UtilForTest.getEntity();
		prod.put("transactionID",tranID);
		prod.put("currency", CurrencyType.CNY.name());
		prod.put("productID","prod1");
		prod.put("_version","1");
		prod.put("payType", PayType.alipay.name());
		prod.put("payStatus", OrderStatus.pending.name());
		prod.put("userID",token);
		prod.put("createdDate",new Date().getTime());
		prod.put("price",price);
		return prod;
	}

	private JSONObject getTransactionUpdateResponse() throws JSONException {

		JSONObject tran= UtilForTest.getEntity();
		tran.put("transactionID",tranID);
		tran.put("currency", CurrencyType.CNY);
		tran.put("productID", "prod1");




		return tran;
	}

	AlipayQueryResult getResult() throws IOException {

		InputStream stream=
				loader.getResource("classpath:com/kii/app/youwill/iap/server/test/DemoAlipayResponse.xml").getInputStream();


		AlipayQueryResult result=new AlipayQueryResult(stream);
		assertEquals("2010073000030344", result.getTradeID());

		assertEquals("1280463992953",result.getTransactionID());

		return result;

	}

	@Test
	public void doFinishOrder() throws Exception {


		when(bucketClient.getDataObject(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("transaction")),
				eq(BucketType.DEFAULT),
				eq(new ObjectID(tranID))
		)).thenReturn(getTransaction());

		when(bucketClient.query(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("configure")),
				eq(BucketType.DEFAULT),
				any(QueryRequest.class))).thenReturn(UtilForTest.getConfigResponse());

		when(bucketClient.query(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("transaction")),
				eq(BucketType.DEFAULT),
				argThat(new QueryReq(false)))).then(new Answer<QueryResponse>(){

			@Override
			public QueryResponse answer(InvocationOnMock invocation) throws Throwable {
				BucketID id= (BucketID) invocation.getArguments()[2];
				if(id.toString().equals("transaction")) {
					return getTransactionResponse();
				}else{
					return null;
				}
			}
		});

		when(accessHelper.queryTransactionStatus(eq(tranID)))
				.thenReturn(getResult());

		when(bucketClient.updateDataObjectPartial(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("transaction")),
				eq(BucketType.DEFAULT),
				eq(new ObjectID(tranID)),
				eq(new EntityTagID("1")),
				argThat(new UpdateReq()))).thenReturn(getTransactionUpdateResponse());




		ObjectCreationResponse resp=new ObjectCreationResponse();
		resp.setObjectID(new ObjectID("newID"));


		when(bucketClient.createDataObject(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("receipt")),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				any(JSONObject.class)) ).thenReturn(resp);

		JSONObject  req=new JSONObject();


		String fullStr="appID"+appID+"secKey"+ UtilForTest.secKey+"orderID"+tranID;

		String sha1Str= DigestUtils.sha1Hex(fullStr);

		req.put("verifySign",sha1Str);

		this.mockMvc.perform(
				post("/iap/finishOrder/order/"+tranID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid",appID)
						.header("x-kii-appkey",appKey)
						.header("Authorization","bear "+token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().isOk())
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(new VerifyResult());
	}


	private static class VerifyResult implements ResultMatcher {


		@Override
		public void match(MvcResult result) throws Exception {

			String context=result.getResponse().getContentAsString();

			JSONObject json=new JSONObject(context);

			assertTrue(json.has("status"));


		}
	}




}
