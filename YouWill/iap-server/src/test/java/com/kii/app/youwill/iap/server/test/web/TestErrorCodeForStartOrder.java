package com.kii.app.youwill.iap.server.test.web;

import com.kii.app.youwill.iap.server.dao.impl.PaypalQueryAccessHelper;
import com.kii.app.youwill.iap.server.entity.ConsumeType;
import com.kii.app.youwill.iap.server.entity.PayType;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.platform.ufp.bucket.*;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.ufe.ObjectCreationResponse;
import com.kii.platform.ufp.ufe.QueryRequest;
import com.kii.platform.ufp.ufe.QueryResponse;
import com.kii.platform.ufp.ufe.errors.BucketNotFoundException;
import org.apache.commons.codec.digest.DigestUtils;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MvcResult;
import org.springframework.test.web.servlet.ResultMatcher;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.content;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

/**
 * Created by ethan on 14-8-25.
 */

public class TestErrorCodeForStartOrder extends BaseWebTest {


	@Autowired
	private PaypalQueryAccessHelper paypalHelper;


	private String appID="mockApp";
	private String appKey="mockKey";

	private String price="12.34";
	private String prodID="prod1";

	private String orderID="order1";


	JSONObject req=new JSONObject();

	String sha1Str=null;

	@Before
	public void init() throws JSONException, BucketNotFoundException {
		super.setup();



		String fullStr="appID"+appID+"secKey"+ UtilForTest.secKey+"productID"+prodID+"payType"+ PayType.alipay.name()+"price"+price;

		sha1Str= DigestUtils.sha1Hex(fullStr);

		req.put("payType", PayType.alipay);
		req.put("verifySign",sha1Str);
		req.put("price",price);

		when(bucketClient.query(
				any(AccessToken.class),
				any(ObjectScope.class),
				any(BucketID.class),
				eq(BucketType.DEFAULT),
				any(QueryRequest.class))).then(new StartAnswer());

		initCreateMock();



	}

	@After
	public void finish(){

		req=new JSONObject();
	}


	@Test
	public void testAPPIDError() throws Exception {


		this.mockMvc.perform(
				post("/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(403))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.APPID_IS_NULL));
	}

	@Test
	public void testAPPKeyError() throws Exception {
		this.mockMvc.perform(
				post( "/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(403))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.APPKEY_IS_NULL));
	}

	@Test
	public void testNoTokenError() throws Exception {

		this.mockMvc.perform(
				post("/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(403))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.TOKEN_IS_NULL));
	}

	@Test
	public void testProdNotFoundError() throws Exception {

		sign = "nullProduct";

		this.mockMvc.perform(
				post( "/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(404))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.PRODUCT_NOT_FOUND));
	}


	@Test
	public void testAlipayMiss() throws Exception {


		sign = "errorAlipay";


		this.mockMvc.perform(
				post("/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(500))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.ALIPAY_CONFIG_MISS));

	}

	@Test
	public void testSecKeyMiss() throws Exception {
		sign = "errorSec";


		this.mockMvc.perform(
				post("/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(500))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.SECURITY_KEY_MISS));
	}


	@Test
	public void testUserNotFound() throws Exception {

		sign = null;

		this.mockMvc.perform(
				post("/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token + " foo")
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(500))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.USER_NOT_FOUND));
	}


	@Test
	public void testSignInvalid() throws Exception {

		req.put("verifySign", sha1Str + "$");


		this.mockMvc.perform(
				post("/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(500))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.SIGN_INVALID));
	}

	@Test
	public void testProductBoughtAlready() throws Exception {
		req.put("verifySign",sha1Str);



		sign="hasReceipt";


		this.mockMvc.perform(
				post("/iap/startOrder/product/" + prodID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(500))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.PRODUCT_BOUGHT_ALREADY));

	}

	private void initCreateMock() throws BucketNotFoundException {
		ObjectCreationResponse resp = new ObjectCreationResponse();
		resp.setObjectID(new ObjectID("newID"));


		when(bucketClient.createDataObject(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("transaction")),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				any(JSONObject.class))).thenReturn(resp);
	}



	private String sign;

	private class StartAnswer implements Answer<QueryResponse> {




		@Override
		public QueryResponse answer(InvocationOnMock invocation) throws Throwable {
			BucketID id= (BucketID) invocation.getArguments()[2];
			if(id.toString().equals("product")){
				final QueryResponse resp= UtilForTest.getProductResponse();
				if("hasReceipt".equals(sign)) {
					JSONObject prod = resp.getResults().getJSONObject(0);
					prod.put("consumeType", ConsumeType.consumable.name());
				}else if("nullProduct".equals(sign)){
					resp.setResults(new JSONArray());
				}
				return resp;

			}else if(id.toString().equals("receipt")){
				final QueryResponse receResp=new QueryResponse();
				receResp.setNextPaginationKey(new PaginationKey(""));

				JSONArray objList=new JSONArray();
				if("hasReceipt".equals(sign)) {
					for (int i = 0; i < 1; i++) {
						JSONObject trans = UtilForTest.getEntity();
						objList.put(trans);
					}
				}
				receResp.setResults(objList);
				return receResp;

			}else if(id.toString().equals("configure")){
				QueryResponse resp= UtilForTest.getConfigResponse();
				if("errorAlipay".equals(sign)) {
					resp.getResults().getJSONObject(0).remove("alipaySecurityKey");
				}else if("errorPaypal".equals(sign)) {
					resp.getResults().getJSONObject(0).remove("paypalClientID");
				}else if("errorSec".equals(sign)) {
					resp.getResults().getJSONObject(0).remove("iapSecurityKey");
				}
				return resp;
			}else{
				return null;
			}
		}
	};


	private VerifyErrorResult getErrorResult(IAPErrorCode code){
		return new VerifyErrorResult(code);
	}
	private static class VerifyErrorResult implements ResultMatcher {


		IAPErrorCode code;
		public VerifyErrorResult(IAPErrorCode code){
			this.code=code;

		}
		@Override
		public void match(MvcResult result) throws Exception {

			String context=result.getResponse().getContentAsString();

			JSONObject json=new JSONObject(context);

			String errorCode=json.getString("errorCode");
			String msg=json.getString("msg");

			assertEquals(errorCode,String.valueOf(code.getCode()));
			assertEquals(msg,code.name());

		}
	}


}
