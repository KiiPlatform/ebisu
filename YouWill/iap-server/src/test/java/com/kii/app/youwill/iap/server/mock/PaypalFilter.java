package com.kii.app.youwill.iap.server.mock;

import com.kii.app.youwill.iap.server.common.IAPUtils;
import com.kii.app.youwill.iap.server.entity.CurrencyType;
import org.apache.commons.codec.binary.Base64;
import org.apache.commons.lang.StringUtils;
import org.apache.commons.lang.time.DateUtils;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import javax.servlet.*;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.Date;
import java.util.Set;
import java.util.concurrent.ConcurrentSkipListSet;

/**
 * Created by ethan on 14-8-29.
 */
public class PaypalFilter implements Filter {
	@Override
	public void init(FilterConfig filterConfig) throws ServletException {

	}

	@Override
	public void doFilter(ServletRequest request, ServletResponse response, FilterChain chain) throws IOException, ServletException {


		String url=request.getServletContext().getContextPath();

		///v1/payments/payment/

		HttpServletRequest httpReq=(HttpServletRequest)request;

		HttpServletResponse httpResp=(HttpServletResponse)request;


		if(url.startsWith("/v1/payments/payment")){



		}else if(url.equals("/v1/oauth2/token")){

			operTokenHeader(httpReq,httpResp);

		}else{
			chain.doFilter(request,response);
		}
		///v1/oauth2/token

	}

	private void operPayQuery(HttpServletRequest request,HttpServletResponse response)throws IOException{

		String contentType=request.getContentType();
		String authInfo=request.getHeader("Authorization");
		String info= StringUtils.substringAfter(authInfo, " ");

		if(!(contentType.equals("application/json")&&
				tokenSet.contains(info)) ) {
			response.setStatus(401);
			return;
		}

		String path=request.getContextPath();

		String paymentID=StringUtils.substringAfterLast(path,"/");

		if(!paymentID.startsWith("pay")){
			response.setStatus(404);
			return;

		}

		JSONObject rootJson=new JSONObject();
		JSONObject payJson=new JSONObject();

		try {
			rootJson.append("payments", payJson);

			payJson.put("id",paymentID);
			Date date=new Date();
			date=DateUtils.addDays(date,-5);
			payJson.put("create_time", IAPUtils.parsePaypalDate(date));
			date= DateUtils.addDays(date,-10);
			payJson.put("update_time",IAPUtils.parsePaypalDate(date));

			payJson.put("status","approved");
			payJson.put("intent","sale");

			JSONObject payInfoJson=new JSONObject();
			payInfoJson.put("payment_method","credit_card");

			payJson.put("payer",payInfoJson);

			JSONObject transJson=new JSONObject();
			payJson.append("transactions",transJson);

			JSONObject amountJson=new JSONObject();

			amountJson.put("total","12.34");
			amountJson.put("currency", CurrencyType.USD);

			transJson.put("amount",amountJson);

			response.getWriter().print(rootJson.toString());

		}catch(JSONException e){
			e.printStackTrace();
			response.setStatus(500);
		}






	}

	private void operTokenHeader(HttpServletRequest request,HttpServletResponse response) throws IOException {


		String contentType=request.getContentType();
		String grant=request.getHeader("grant_type");

		if (!( contentType.equals("application/json")&&
				grant.equals("client_credentials") ) ){
			response.setStatus(401);
			return;
		};

		String authInfo=request.getHeader("Authorization");
		String info= StringUtils.substringAfter(authInfo," ");

		String decodeInfo= new String(Base64.decodeBase64(info),"UTF-8");


		String user=StringUtils.substringBefore(decodeInfo, ":");
		String pwd=StringUtils.substringAfter(decodeInfo,":");

		if(!user.equals(pwd)){
			response.setStatus(403);
			return;
		}

		String token=user+pwd+"token";
		tokenSet.add(token);

		/*
		{
  "scope": "https://api.paypal.com/v1/payments/.* https://api.paypal.com/v1/vault/credit-card https://api.paypal.com/v1/vault/credit-card/.*",
  "access_token": "EEwJ6tF9x5WCIZDYzyZGaz6Khbw7raYRIBV_WxVvgmsG",
  "token_type": "Bearer",
  "app_id": "APP-6XR95014BA15863X",
  "expires_in": 28800
}
		 */

		try {
			JSONObject json = new JSONObject();
			json.put("scope", "mock");
			json.put("access_token", token);
			json.put("token_type", "Bearer");
			json.put("app_id", "MockAppID");
			json.put("expires_in", 28800);

			response.getWriter().print(json.toString());
		}catch(JSONException e){
			e.printStackTrace();
			response.setStatus(500);
		}



	}

	private Set<String> tokenSet=new ConcurrentSkipListSet<String>();


	@Override
	public void destroy() {

	}
}
