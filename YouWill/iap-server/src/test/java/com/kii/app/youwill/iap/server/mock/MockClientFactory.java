package com.kii.app.youwill.iap.server.mock;

import com.kii.app.youwill.iap.server.dao.impl.AlipayQueryAccessHelper;
import com.kii.app.youwill.iap.server.dao.impl.PaypalQueryAccessHelper;
import com.kii.app.youwill.iap.server.entity.AlipayQueryResult;
import com.kii.app.youwill.iap.server.entity.PaypalQueryResult;
import com.kii.app.youwill.iap.server.test.web.UtilForTest;
import com.kii.platform.ufp.app.AppID;
import com.kii.platform.ufp.errors.UserNotFoundException;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.oauth2.ClientID;
import com.kii.platform.ufp.oauth2.ClientSecret;
import com.kii.platform.ufp.ufe.UserDataRetrievalResponse;
import com.kii.platform.ufp.ufe.client.http.stateless.ACLClient;
import com.kii.platform.ufp.ufe.client.http.stateless.AuthClient;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.client.http.stateless.UserClient;
import com.kii.platform.ufp.ufe.errors.AppConfigParameterNotFoundException;
import com.kii.platform.ufp.user.UserID;
import org.apache.commons.io.IOUtils;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.io.ResourceLoader;

import java.io.IOException;
import java.io.InputStream;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;

/**
 * Created by ethan on 14-8-19.
 */
@Configuration
public class MockClientFactory {

	@Autowired
	private ResourceLoader loader;


	@Bean
	public AlipayQueryAccessHelper getMockAlipayHelper() throws IOException {

		InputStream stream=
				loader.getResource("classpath:com/kii/app/youwill/iap/server/test/DemoAlipayResponse.xml").getInputStream();


		AlipayQueryResult result=new AlipayQueryResult(stream);

		AlipayQueryAccessHelper helper= Mockito.mock(AlipayQueryAccessHelper.class);


		when(helper.queryTransactionStatus(any(String.class))).thenReturn(result);

		return helper;
	}


@Bean
	public PaypalQueryAccessHelper getMockPaypalHelper() throws JSONException, IOException {


		String str= IOUtils.toString(loader.getResource("classpath:com/kii/app/youwill/iap/server/test/DemoPaypalResponse.json").getInputStream(), "UTF-8");
		JSONObject json=new JSONObject(str);

		PaypalQueryResult result=new PaypalQueryResult(json);


		PaypalQueryAccessHelper helper= Mockito.mock(PaypalQueryAccessHelper.class);

		when(helper.doPaypalQuery(any(String.class)))
				.thenReturn(result);

		return helper;
	}

	@Bean
	public UserClient getUserClient() throws UserNotFoundException {

		UserClient client= 	 Mockito.mock(UserClient.class);



		when(client.getUser(
				any(AccessToken.class)
		)).then(new Answer<UserDataRetrievalResponse>() {

			@Override
			public UserDataRetrievalResponse answer(InvocationOnMock invocation) throws Throwable {
				Object[] array = invocation.getArguments();

				AccessToken token = (AccessToken) array[0];

				if (token.equals(new AccessToken(UtilForTest.token))) {

					UserDataRetrievalResponse resp = new UserDataRetrievalResponse();
					resp.setUserID(new UserID(UtilForTest.token));

					return resp;
				} else {
					throw new UserNotFoundException(UtilForTest.token);
				}

			}
		});


		return client;

	}

	@Bean
	public BucketClient getBucketClient() {

		return 	 Mockito.mock(BucketClient.class);
	}

	@Bean
	public ACLClient getACLClient() {

		return Mockito.mock(ACLClient.class);
	}

	@Bean
	public AuthClient getAuthClient() throws AppConfigParameterNotFoundException {

		AuthClient auth=Mockito.mock(AuthClient.class);

		AuthClient.AdminAuthResponse resp=new AuthClient.AdminAuthResponse(new AccessToken("adminToken"),null);

		when(auth.loginAsAppAdmin(eq(new AppID("mockAppID")),eq(new ClientID("clientID")),eq(new ClientSecret("secret")))).thenReturn(resp);

		return auth;
	}
}
