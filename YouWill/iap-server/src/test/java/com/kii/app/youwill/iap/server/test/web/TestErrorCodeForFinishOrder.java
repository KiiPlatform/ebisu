package com.kii.app.youwill.iap.server.test.web;

import com.kii.app.youwill.iap.server.dao.impl.PaypalQueryAccessHelper;
import com.kii.app.youwill.iap.server.entity.OrderStatus;
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

public class TestErrorCodeForFinishOrder extends BaseWebTest {



	@Autowired
	private PaypalQueryAccessHelper paypalHelper;


	private String appID="mockApp";
	private String appKey="mockKey";

	private String price="12.34";
	private String prodID="prod1";

	private String orderID="order1";

	String sign;

	JSONObject req = new JSONObject();


	@Before
	public void init() throws JSONException, BucketNotFoundException {
		super.setup();





		String fullStr = "appID" + appID + "secKey" + UtilForTest.secKey + "orderID" + orderID;

		String sha1Str = DigestUtils.sha1Hex(fullStr);

		req.put("verifySign", sha1Str+"foo");


		initFinishMock();

		when(bucketClient.query(
				any(AccessToken.class),
				any(ObjectScope.class),
				any(BucketID.class),
				eq(BucketType.DEFAULT),
				any(QueryRequest.class))).then(new FinishAnswer());

	}

	@After
	public void finish(){
		req = new JSONObject();

	}


	private class  FinishAnswer implements Answer<QueryResponse>{


		@Override
		public QueryResponse answer(InvocationOnMock invocation) throws Throwable {
			BucketID id= (BucketID) invocation.getArguments()[2];
			if(id.toString().equals("transaction")){
				final QueryResponse resp= UtilForTest.getTransactionResponse();
				if("nullOrder".equals(sign)) {
					resp.setResults(new JSONArray());
				}else if("wrongOrder".equals(sign)){
					JSONObject obj=resp.getResults().getJSONObject(0);
					obj.put("payStatus",OrderStatus.completed.name());
				}else if("errorPaypal".equals(sign)){
					JSONObject obj=resp.getResults().getJSONObject(0);
					obj.put("payType",PayType.paypal.name());
				}
				return resp;

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
	}


	private void initFinishMock() throws BucketNotFoundException {
		ObjectCreationResponse resp = new ObjectCreationResponse();
		resp.setObjectID(new ObjectID("newID"));


		when(bucketClient.createDataObject(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("receipt")),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				any(JSONObject.class))).thenReturn(resp);
	}



    @Test
	public void testSignInvalid() throws Exception {


		this.mockMvc.perform(
				post("/iap/finishOrder/order/" + orderID)
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
	public void testOrderMiss() throws Exception {


		sign = "nullOrder";

		String fullStr = "appID" + appID + "secKey" + UtilForTest.secKey + "orderID" + orderID;

		String sha1Str = DigestUtils.sha1Hex(fullStr);

		req.put("verifySign", sha1Str);


		this.mockMvc.perform(
				post("/apps/" + appID + "/iap/finishOrder/order/" + orderID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(404))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.ORDER_NOT_FOUND));
	}

	@Test
	public void testPayStatus() throws Exception {
		sign = "wrongOrder";

		String fullStr = "appID" + appID + "secKey" + UtilForTest.secKey + "orderID" + orderID;

		String sha1Str = DigestUtils.sha1Hex(fullStr);

		req.put("verifySign", sha1Str);


		this.mockMvc.perform(
				post("/apps/" + appID + "/iap/finishOrder/order/" + orderID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid", appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization", "bear " + UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(500))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.PAY_STATUS_INVALID));
	}

	@Test
	public void testPayInfo() throws Exception {

		sign="errorPaypal";

		String fullStr = "appID" + appID + "secKey" + UtilForTest.secKey + "orderID" + orderID;

		String sha1Str = DigestUtils.sha1Hex(fullStr);

		req.put("verifySign", sha1Str);
//		req.put("paymentID","foo");

		this.mockMvc.perform(
				post("/iap/finishOrder/order/" + orderID)
						.content(req.toString())
						.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
						.header("x-kii-appid",appID)
						.header("x-kii-appkey", appKey)
						.header("Authorization","bear "+ UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().is(500))
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(getErrorResult(IAPErrorCode.PAYID_NOT_FOUND));



	}

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
