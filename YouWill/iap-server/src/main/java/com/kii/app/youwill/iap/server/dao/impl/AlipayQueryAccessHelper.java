package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.common.CliperSpec;
import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.entity.AlipayConfig;
import com.kii.app.youwill.iap.server.entity.AlipayQueryResult;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import javax.annotation.PostConstruct;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * Created by ethan on 14-8-13.
 */
@Component
public class AlipayQueryAccessHelper {

/*

https://mapi.alipay.com/gateway.do?
service=single_trade_query&
sign=d8ed9f0 15214e7cd59bfadb6c945a87b&
trade_no=2010121502730740&
partner=208800 1159940003&
out_trade_no=2109095506028810&
sign_type=MD5

 */

	@Autowired
	private ConfigInfoStore store;


	private CloseableHttpClient httpClient;

	@PostConstruct
	public void init(){
		 httpClient = HttpClients.createDefault();

	}

	public AlipayQueryResult queryTransactionStatus(String transactionID){


		AlipayConfig  config=store.getAlipayConfig();


		Map<String,String> paramMap=new HashMap<String,String>();

		paramMap.put("service","single_trade_query");
		paramMap.put("out_trade_no",transactionID);
		paramMap.put("partner",config.getPartnerID());
		paramMap.put("_input_charset","UTF-8");


		String orginStr= CliperSpec.getSignReadyString(paramMap);

		String sign=config.signWithMD5(orginStr);

		paramMap.put("sign",sign);
		paramMap.put("sign_type","MD5");

		String fullUrl=getFullPath(paramMap);

		HttpPost post=new HttpPost(fullUrl);

		AlipayQueryResult result=null;
		try {
			CloseableHttpResponse response = httpClient.execute(post);

			InputStream stream=response.getEntity().getContent();


			 result=new AlipayQueryResult(stream);

			if(!result.isSuccess()){
				throw new ServiceException(IAPErrorCode.ALIPAY_SERVICE_INVALID);
			}

			if(!verify(config, result)){
				result=null;
			};


		} catch (IOException e) {
			e.printStackTrace();
		}finally{
			post.releaseConnection();
		}

		return result;


	}

	private boolean verify(AlipayConfig config, AlipayQueryResult result) {
		if("MD5".equals(result.getSignType())) {
			return config.verifyWithMD5(result.getSignReadyStr(), result.getSign());
		}else{
			return config.verifyWithRSA(result.getSignReadyStr(),result.getSign());
		}
	}

	@Value("${iap-server.pay-server.end-point.domain.alipay}")
	private String  baseUrl;


	private String  getFullPath(Map<String,String> params){

		StringBuilder sb=new StringBuilder(baseUrl);
		sb.append("?");
		for(Map.Entry<String,String> entry:params.entrySet()){
			sb.append(entry.getKey()).append("=").append(entry.getValue()).append("&");
		}

		sb.deleteCharAt(sb.length()-1);

		return sb.toString();

	}
}
