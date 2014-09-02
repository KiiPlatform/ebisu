package com.kii.app.youwill.iap.server.test.web;

import com.kii.app.youwill.iap.server.entity.ConsumeType;
import com.kii.app.youwill.iap.server.entity.CurrencyType;
import com.kii.app.youwill.iap.server.entity.OrderStatus;
import com.kii.app.youwill.iap.server.entity.PayType;
import com.kii.platform.ufp.bucket.PaginationKey;
import com.kii.platform.ufp.ufe.QueryResponse;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.test.web.servlet.MvcResult;
import org.springframework.test.web.servlet.ResultMatcher;

import java.util.Date;

/**
 * Created by ethan on 14-8-21.
 */
public class UtilForTest {

	public static  String secKey="secKey";


	public static String token="mockToken";


	public static  String price="12.34";
	public static  String prodID="prod1";


	public static JSONObject getEntity() throws JSONException {
		JSONObject prod=new JSONObject();

		prod.put("_id","uuid-id");
		prod.put("_version",1);
		prod.put("_owner","abc");
		prod.put("_created",new Date().getTime());
		prod.put("_modified",new Date().getTime());

		return prod;

	}

	public static  QueryResponse getTransactionResponse() throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();

		JSONObject tran= UtilForTest.getEntity();
		tran.put("transactionID","tran1");
		tran.put("price", "34.12");

		tran.put("userID", token);
		tran.put("payStatus", OrderStatus.pending.name());

		tran.put("payType", PayType.alipay.name());

		tran.put("currency", CurrencyType.CNY.name());
		tran.put("productID", "prod1");

		tran.put("createdDate",new Date().getTime());

		objList.put(tran);
		resp.setResults(objList);
		return resp;
	}



	public static  QueryResponse getProductResponse() throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();

		JSONObject prod= UtilForTest.getEntity();
		prod.put("productID",prodID);
		prod.put("price",price);
		prod.put("consumeType", ConsumeType.periodical);

		objList.put(prod);
		resp.setResults(objList);
		return resp;
	}


	public static QueryResponse getConfigResponse() throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();
		JSONObject config= UtilForTest.getEntity();

		config.put("alipayPartnerID", "alipayPartner");
		config.put("alipayRSAPrivateKey", "MIICdQIBADANBgkqhkiG9w0BAQEFAASCAl8wggJbAgEAAoGBAILQ52NUh+tGyBmD/+tNdpsQimrCq5vFRQ1qdDq86v01SYU7X7aArVVliCMjRCHzXlG1wqLIsdj2DCwFpJMvQIUPAn1ESxHjpJRk+BzOq9YHt0RUtCW/Zh/vxXk8KJFwXscHUiZryefEk8lFYqSjtSi1/+/OjEmM7ObrBcnz03nTAgMBAAECgYB9k4yMDdlsQcZUuCIz/2ib8oTtd3LFVdmzxz1MJY5vA3hQLW70AxJhp9XH5VLCRJ2GUrVFDuOrIocMu14A7JtBs8doXShhZ489FJcp2qSwyxM3IHF94+ydganubErBuqiWeDsO1JbSaOqiSsOqOAHUebIwlJcn47FLR9Qnmth+AQJBAO906qsaqyc5dfZpyqpF5YlF5Dc2eRZMSoP0HOtodT60Ck84onem6GmsjSaD9tkSUbOFqjw2you+Mp0XmOlH548CQQCL2otrBnp2r7j62AcyZu9CAedTwUMZsWWCcdk65vypB+aDrrYnBvkbHFD/dtAvgKxRXCaNnWNrmD8j36zQNYd9AkB1eN8TF981O/xB6b5x8B5eTLpb185pgP3hZCW32yQBoeOY38jceDZ2eR858O1tqo4ISx+rsJmHGsTLymLKturDAkAih5oGmbng1fARFwdn3VxTas7nsf7UOCV8+IkO7vsQ7N9wQdG9fw5AMFSbToWGRPnGuL7YFjT22sc0H0S2v5CtAkA9wBqYrlsEKVmQQvdcRS2666CLpf3nqpQci8CsV8nE7cdptfgduo+NyzOj6v48Cyz7c21sKM8Q91F0pObE8CAr");
		config.put("alipaySecurityKey", "alipaySecKey");
		config.put("alipaySellerID", "alipaySeller");
		config.put("paypalSandbox", "paypaySand");
		config.put("paypalSandboxSecret", "paypaySand");
		config.put("paypalClientID", "paypay");
		config.put("paypalSecret", "paypay");

		config.put("iapSecurityKey", secKey);

		objList.put(config);
		resp.setResults(objList);
		return resp;
	}


	public  static class DisplayResult implements ResultMatcher {


		@Override
		public void match(MvcResult result) throws Exception {

			String context=result.getResponse().getContentAsString();
			System.out.println("context:"+context);
		}
	}


}
