package com.kii.app.youwill.iap.server.web;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.servlet.handler.HandlerInterceptorAdapter;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

/**
 * Created by ethan on 14-8-4.
 */

public class KiiAppInterceptorAdapter  extends HandlerInterceptorAdapter {

	@Autowired
	private AppContext ctx;




	@Override
	public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler)
			throws Exception {


		if(request.getMethod().equals("POST")
				||request.getMethod().equals("GET")) {


			ctx.bindRequest(request);
			response.setHeader("Access-Control-Allow-Origin","*");


		}


		return true;
	}

	@Override
	public void afterCompletion(
			HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex)
			throws Exception {



	}



}
