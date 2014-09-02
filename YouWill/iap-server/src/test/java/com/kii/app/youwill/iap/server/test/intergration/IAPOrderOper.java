package com.kii.app.youwill.iap.server.test.intergration;

import com.kii.app.youwill.iap.server.common.StringTemplate;
import com.kii.app.youwill.iap.server.entity.OrderStatus;
import com.kii.app.youwill.iap.server.entity.PayType;
import com.kii.platform.ufp.oauth2.AccessToken;
import org.apache.commons.codec.digest.DigestUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.StringUtils;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import java.io.InputStream;
import java.net.URI;

/**
 * Created by ethan on 14-8-27.
 */
public class IAPOrderOper {

	private String userSecKey;

	private CloseableHttpClient httpClient;

//	private HttpPost post=new HttpPost();

	private String baseHost;

	public IAPOrderOper(String baseHost){

		this.baseHost=baseHost;
		httpClient = HttpClients.createDefault();

	}

	private String appID;

	private String appKey;

	private AccessToken token;

	public void initAppInfo(KiiCloudInfo info,AccessToken token,String secKey){
/*
      this.headers['x-kii-appid'] = Kii.getAppID();
      this.headers['x-kii-appkey'] = Kii.getAppKey();
      if (this.accept != null) {
        this.headers['accept'] = this.accept;
      }
      this.headers['x-kii-path'] = this.path;
      if (!this.anonymous) {
        this.headers['Authorization'] = "Bearer " + accessToken;

 */
		this.appID=info.appID;
		this.appKey=info.appKey;
		this.userSecKey=secKey;
		this.token=token;


	}

	public CreateOrderResponse doStartOrder(String productID,PayType payType){
			return doStartOrder(productID,payType,null,null);
	}

	public CreateOrderResponse doStartOrder(String productID,PayType payType,String price){

			return doStartOrder(productID,payType,price,null);
	}


	private String CREATE_ORDER="http://${2}/api/apps/${0}/iap/startOrder/product/${1}";

	public CreateOrderResponse doStartOrder(String productID,PayType payType,String price,String transactionID)  {

		HttpPost post=new HttpPost();

		post.addHeader("x-kii-appid",appID);
		post.addHeader("x-kii-appkey",appKey);
		post.addHeader("Authorization","Bearer "+token.toString());
		post.addHeader("Content-Type","application/json");

		StringBuffer sb=new StringBuffer();
		sb.append("appID").append(appID)
				.append("secKey").append(userSecKey)
				.append("productID").append(productID)
				.append("payType").append(payType.name());
		if(StringUtils.isNotBlank(price)){
			sb.append("price").append(price);
		}
		if(StringUtils.isNotBlank(transactionID)){
			sb.append(transactionID).append(transactionID);
		}
//		sb.append(userSecKey);

		String verifySign= DigestUtils.sha1Hex(sb.toString());

		JSONObject json=new JSONObject();

		try {
			json.put("payType", payType.name());
			json.put("verifySign", verifySign);
			if (StringUtils.isNotBlank(price)) {
				json.put("price", price);
			}
			if (StringUtils.isNotBlank(transactionID)) {
				json.put("transactionID", transactionID);
			}
		}catch(JSONException e){
			throw new IllegalArgumentException(e);
		}

		String jsonStr=json.toString();

		String urlStr= StringTemplate.gener(CREATE_ORDER,appID,productID,baseHost);
		try {

			URI uri=new URI(urlStr);
			post.setURI(uri);

			post.setEntity(new StringEntity(jsonStr));

			CloseableHttpResponse response = httpClient.execute(post);

			InputStream stream=response.getEntity().getContent();

			String jsonContext= IOUtils.toString(stream, "UTF-8");


			JSONObject jsonResp=new JSONObject(jsonContext);

			return new CreateOrderResponse(jsonResp);

		} catch (Exception e) {
			e.printStackTrace();
			throw new IllegalArgumentException(e);
		}finally{
			post.releaseConnection();
		}
	}

	private String FINISH_ORDER="http://${2}/api/apps/${0}/iap/finishOrder/order/${1}";


	public  OrderStatus doFinishAlipayOrder(String orderID){

		StringBuffer sb=new StringBuffer();
		sb.append("appID").append(appID)
				.append("secKey").append(userSecKey)
				.append("orderID").append(orderID);
//		sb.append(userSecKey);

		String verifySign= DigestUtils.sha1Hex(sb.toString());

		JSONObject json=new JSONObject();

		try {
			json.put("verifySign", verifySign);
		}catch(JSONException e){
			throw new IllegalArgumentException(e);
		}

		return doFinishOrder(json,orderID);

	}

	private  OrderStatus doFinishOrder(JSONObject json,String orderID){



		HttpPost post=new HttpPost();

		post.addHeader("x-kii-appid",appID);
		post.addHeader("x-kii-appkey",appKey);
		post.addHeader("Authorization","Bearer "+token);
		post.addHeader("Content-Type","application/json");



		String jsonStr=json.toString();

		String urlStr= StringTemplate.gener(FINISH_ORDER,appID,orderID,baseHost);
		try {

			URI uri=new URI(urlStr);
			post.setURI(uri);

			post.setEntity(new StringEntity(jsonStr));

			CloseableHttpResponse response = httpClient.execute(post);

			InputStream stream=response.getEntity().getContent();

			String str= IOUtils.toString(stream, "UTF-8");

			JSONObject respJson=new JSONObject(str);


			return OrderStatus.valueOf(respJson.getString("status"));

		} catch (Exception e) {
			e.printStackTrace();
			throw new IllegalArgumentException(e);
		}finally{
			post.releaseConnection();
		}


	}

	public OrderStatus doFinishOrderInPaypal(String orderID,String paymentID){

		StringBuffer sb=new StringBuffer();
		sb.append("appID").append(appID)
				.append("appKey").append(appKey)
				.append("orderID").append(orderID)
				.append("paymentID").append(paymentID);
		sb.append(userSecKey);

		String verifySign= DigestUtils.sha1Hex(sb.toString());

		JSONObject json=new JSONObject();

		try {
			json.put("verifySign", verifySign);
			json.put("paymentID",paymentID);
		}catch(JSONException e){
			throw new IllegalArgumentException(e);
		}

		return doFinishOrder(json,orderID);


	}
}
