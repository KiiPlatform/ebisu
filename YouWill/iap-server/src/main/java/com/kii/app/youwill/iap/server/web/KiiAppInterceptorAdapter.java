package com.kii.app.youwill.iap.server.web;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.servlet.handler.HandlerInterceptorAdapter;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

/**
 * Created by ethan on 14-8-4.
 */

public class KiiAppInterceptorAdapter extends HandlerInterceptorAdapter {

    private static String ALIPAY_CALLBACK_URL = "/iap/callback/alipay/";

    @Autowired
    private AppContext ctx;


    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler)
            throws Exception {
        if (isIAPCallback(request)) {
            ctx.initWithAdmin();
        } else if (request.getMethod().equals("POST")
                || request.getMethod().equals("GET")) {
            ctx.bindRequest(request);
            response.setHeader("Access-Control-Allow-Origin", "*");
        }
        return true;
    }

    private boolean isIAPCallback(HttpServletRequest request) {
        String requestURI = request.getRequestURI();
        String callBackURL = request.getContextPath() + ALIPAY_CALLBACK_URL;
        return requestURI.startsWith(callBackURL);
    }

    @Override
    public void afterCompletion(
            HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex)
            throws Exception {


    }


}
