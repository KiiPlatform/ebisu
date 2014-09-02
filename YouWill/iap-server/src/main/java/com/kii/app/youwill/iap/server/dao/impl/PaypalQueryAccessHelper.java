package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.entity.PaypalConfig;
import com.kii.app.youwill.iap.server.entity.PaypalQueryResult;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.app.youwill.iap.server.web.AppContext;
import org.apache.commons.io.IOUtils;
import org.apache.http.NameValuePair;
import org.apache.http.auth.AuthScope;
import org.apache.http.auth.UsernamePasswordCredentials;
import org.apache.http.client.CredentialsProvider;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.BasicCredentialsProvider;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.message.BasicNameValuePair;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by ethan on 14-8-14.
 */
@Component
public class PaypalQueryAccessHelper {
	@Autowired
	private AppContext appContext;
	@Autowired
	private ConfigInfoStore store;


	public  PaypalQueryResult doPaypalQuery(String paymentID){

		String token=doPaypalAuth();

		CloseableHttpClient httpClient = HttpClients.createDefault();

		HttpGet post=new HttpGet(store.getPaypalDomain()+"/v1/payments/payment/"+paymentID);

		post.addHeader("Content-Type","application/json");
		post.addHeader("Authorization","Bearer "+token);
		/*
		-H "Content-Type:application/json" \
-H "Authorization: Bearer <Access-Token>"

		 */
		try {

			CloseableHttpResponse response = httpClient.execute(post);

			if(response.getStatusLine().getStatusCode()!=200){

				if(response.getStatusLine().getStatusCode()==404){
					throw new ServiceException(IAPErrorCode.PAYMENT_ID_ERROR);
				}


				throw new ServiceException(IAPErrorCode.PAYPAL_SERVICE_INVALID);

			};


			InputStream stream=response.getEntity().getContent();

			String json= IOUtils.toString(stream, "UTF-8");

			JSONObject jsonObj=new JSONObject(json);



			return new PaypalQueryResult(jsonObj);


		} catch (IOException e) {
			throw new ServiceException(IAPErrorCode.PAYPAL_SERVICE_INVALID);
		} catch (JSONException ex) {
			throw new ServiceException(IAPErrorCode.PAYPAL_SERVICE_INVALID);
		} finally{
			post.releaseConnection();
		}


	}

	private  String doPaypalAuth(){

		PaypalConfig paypal=store.getPaypalConfig();

		CredentialsProvider credsProvider = new BasicCredentialsProvider();

		credsProvider.setCredentials(
				AuthScope.ANY,
				new UsernamePasswordCredentials(paypal.getClientID(), paypal.getSecret()));


		CloseableHttpClient httpClient = HttpClients.custom()
				.setDefaultCredentialsProvider(credsProvider)
				.build();

		HttpPost post=new HttpPost(store.getPaypalDomain()+"/v1/oauth2/token");

		List<NameValuePair> nvps = new ArrayList<NameValuePair>();
		nvps.add(new BasicNameValuePair("grant_type", "client_credentials"));

		post.addHeader("Content-Type","application/json");
		post.addHeader("Accept-Language","UTF-8");


		try {
			post.setEntity(new UrlEncodedFormEntity(nvps,"UTF-8"));

			CloseableHttpResponse response = httpClient.execute(post);

			InputStream stream=response.getEntity().getContent();

			String json= IOUtils.toString(stream, "UTF-8");

			JSONObject jsonObj=new JSONObject(json);

			String token= jsonObj.getString("access_token");

			return token;


		} catch (IOException e) {
			throw new ServiceException(IAPErrorCode.PAYPAL_SERVICE_INVALID);
		} catch (JSONException ex) {
			throw new ServiceException(IAPErrorCode.PAYPAL_SERVICE_INVALID);
		} finally{
			post.releaseConnection();
		}

	}


}
