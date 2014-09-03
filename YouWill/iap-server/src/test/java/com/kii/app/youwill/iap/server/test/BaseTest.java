package com.kii.app.youwill.iap.server.test;

import com.kii.app.youwill.iap.server.web.AppContext;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.ApplicationContext;
import org.springframework.test.context.ContextConfiguration;
import org.springframework.test.context.junit4.SpringJUnit4ClassRunner;

import javax.servlet.http.HttpServletRequest;
import java.util.Date;

import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;

/**
 * Created by ethan on 14-8-18.
 */

@RunWith(SpringJUnit4ClassRunner.class)
@ContextConfiguration(locations={"classpath:com/kii/app/youwill/iap/server/test/unitTestContext.xml"})
public abstract  class BaseTest  {

	@Autowired
	protected AppContext appContext;

	@Autowired
	private ApplicationContext ctx;

//
//
	protected String appID="mockAppID";
//
	protected String token="mockToken";
//
	protected String appKey="mockAppKey";



	public void initRequestWithSandbox(){


//		ctx.getEnvironment().acceptsProfiles("test");


		HttpServletRequest request= Mockito.mock(HttpServletRequest.class);

//		when(request.getHeader(eq("x-kii-appid"))).thenReturn(appID);
//
//		when(request.getHeader(eq("x-kii-appkey"))).thenReturn(appKey);

		when(request.getHeader(eq("Authorization"))).thenReturn("Bears "+token);

		when(request.getRequestURI()).thenReturn("/sandbox/mock");


		appContext.bindRequest(request);


	}


	public void initRequest(){


		ctx.getEnvironment().acceptsProfiles("test");


		HttpServletRequest request= Mockito.mock(HttpServletRequest.class);

//		when(request.getHeader(eq("x-kii-appid"))).thenReturn(appID);
//
//		when(request.getHeader(eq("x-kii-appkey"))).thenReturn(appKey);

		when(request.getHeader(eq("Authorization"))).thenReturn("Bears "+token);

		when(request.getRequestURI()).thenReturn("/mock");


		appContext.bindRequest(request);

	}


	protected JSONObject getEntity() throws JSONException {
		JSONObject prod=new JSONObject();

		prod.put("_id","uuid-id");
		prod.put("_version",1);
		prod.put("_owner","abc");
		prod.put("_created",new Date().getTime());
		prod.put("_modified",new Date().getTime());

		return prod;

	}


}
