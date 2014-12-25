package com.kii.app.youwill.iap.server.unionpay.util;


import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.ResponseHandler;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;

import java.io.IOException;
import java.io.UnsupportedEncodingException;

/**
 * 类名：https/https报文发送处理类
 * 功能：https/https报文发送处理
 * 版本：1.0
 * 日期：2012-10-11
 * 作者：中国银联UPMP团队
 * 版权：中国银联
 * 说明：以下代码只是为了方便商户测试而提供的样例代码，商户可以根据自己的需要，按照技术文档编写,并非一定要使用该代码。该代码仅供参考。
 */
public class HttpUtil {

    public static String encoding;

    private static final HttpClient client;

    private static final RequestConfig requestConfig;

    static {

        client = HttpClients.createDefault();

        requestConfig = RequestConfig.copy(RequestConfig.DEFAULT)
                .setConnectionRequestTimeout(15000)
                .setSocketTimeout(30000)
                .setStaleConnectionCheckEnabled(true)
                .build();


    }


    public static String post(String url, String encoding, String content) {
        try {
            String resp = post(url, new StringEntity(content, encoding));
            return resp;
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }


    public static String post(String url, String content) {
        return post(url, encoding, content);
    }


    public static String post(String url, HttpEntity requestEntity) throws Exception {
        HttpPost post = new HttpPost(url);
        post.setConfig(requestConfig);
        post.setEntity(requestEntity);
        String responseBody = client.execute(post, responseHandler);
        return responseBody;
    }

    static ResponseHandler<String> responseHandler = new ResponseHandler<String>() {
        @Override
        public String handleResponse(HttpResponse httpResponse) throws ClientProtocolException, IOException {
            int status = httpResponse.getStatusLine().getStatusCode();
            if (status == HttpStatus.SC_OK) {
                HttpEntity entity = httpResponse.getEntity();
                return entity != null ? EntityUtils.toString(entity) : null;
            } else {
                throw new ClientProtocolException("Unexpected response status: " + status);
            }
        }
    };
}
