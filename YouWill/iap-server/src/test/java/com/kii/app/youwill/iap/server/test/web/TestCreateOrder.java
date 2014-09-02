package com.kii.app.youwill.iap.server.test.web;

import com.kii.app.youwill.iap.server.entity.PayType;
import com.kii.platform.ufp.bucket.*;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.ufe.ObjectCreationResponse;
import com.kii.platform.ufp.ufe.QueryRequest;
import org.apache.commons.codec.digest.DigestUtils;
import org.codehaus.jettison.json.JSONObject;
import org.junit.Test;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MvcResult;
import org.springframework.test.web.servlet.ResultMatcher;

import static junit.framework.TestCase.assertTrue;
import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.content;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

/**
 * Created by ethan on 14-8-21.
 */

public class TestCreateOrder extends BaseWebTest {

//	private String BUCKET="configure";


	private static class VerifyResult implements ResultMatcher{


		@Override
		public void match(MvcResult result) throws Exception {

			String context=result.getResponse().getContentAsString();

			JSONObject json=new JSONObject(context);

			assertTrue(json.has("transactionID"));

			assertTrue(json.has("payInfo"));

			JSONObject  pay=json.getJSONObject("payInfo");

			assertEquals(pay.getString("alipayPartnerID"),"alipayPartner");

			assertEquals(pay.getString("alipaySecurityKey"),"alipaySecKey");


		}
	}






	private String appID="mockApp";
	private String appKey="mockKey";





	@Test
	public void doStartProduct() throws Exception {


		when(bucketClient.query(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("configure")),
				eq(BucketType.DEFAULT),
				any(QueryRequest.class))).thenReturn(UtilForTest.getConfigResponse());

		when(bucketClient.query(
					any(AccessToken.class),
					any(ObjectScope.class),
					eq(new BucketID("product")),
					eq(BucketType.DEFAULT),
					any(QueryRequest.class))).thenReturn(UtilForTest.getProductResponse());

		ObjectCreationResponse resp=new ObjectCreationResponse();
		resp.setObjectID(new ObjectID("newID"));


		when(bucketClient.createDataObject(
				any(AccessToken.class),
				any(ObjectScope.class),
				eq(new BucketID("transaction")),
				eq(BucketType.DEFAULT),
				eq(DataType.APPLICATION_JSON),
				any(JSONObject.class))).thenReturn(resp);

		JSONObject  req=new JSONObject();


		String fullStr="appID"+appID+"secKey"+ UtilForTest.secKey+"productID"+ UtilForTest.prodID+"payType"+PayType.alipay.name()+"price"+ UtilForTest.price;

		String sha1Str= DigestUtils.sha1Hex(fullStr);

		req.put("payType", PayType.alipay);
		req.put("verifySign",sha1Str);
		req.put("price", UtilForTest.price);



		this.mockMvc.perform(
				post("/apps/"+appID+"/iap/startOrder/product/"+ UtilForTest.prodID)
				.content(req.toString())
				.accept(MediaType.parseMediaType("application/json;charset=UTF-8"))
				.header("x-kii-appid",appID)
				.header("x-kii-appkey",appKey)
				.header("Authorization","bear "+ UtilForTest.token)
		)
				.andExpect(new UtilForTest.DisplayResult())
				.andExpect(status().isOk())
				.andExpect(content().contentType("application/json;charset=UTF-8"))
				.andExpect(new VerifyResult());





		}




}
