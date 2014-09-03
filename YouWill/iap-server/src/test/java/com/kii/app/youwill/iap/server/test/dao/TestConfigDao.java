package com.kii.app.youwill.iap.server.test.dao;

import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.test.BaseTest;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.BucketID;
import com.kii.platform.ufp.bucket.BucketType;
import com.kii.platform.ufp.bucket.ObjectScope;
import com.kii.platform.ufp.bucket.PaginationKey;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.ufe.QueryRequest;
import com.kii.platform.ufp.ufe.QueryResponse;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.errors.BucketNotFoundException;
import com.kii.platform.ufp.ufe.query.clauses.AllClause;
import com.kii.platform.ufp.ufe.query.clauses.Clause;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.Test;
import org.mockito.ArgumentMatcher;
import org.springframework.beans.factory.annotation.Autowired;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.when;

/**
 * Created by ethan on 14-8-21.
 */
public class TestConfigDao extends BaseTest {


	private String BUCKET="configure";

	@Autowired
	private BucketClient bucketClient;

	@Autowired
	private AppContext appContext;


	@Autowired
	private ConfigInfoStore store;

	class QueryReq extends ArgumentMatcher<QueryRequest> {


		@Override
		public boolean matches(Object argument) {

			QueryRequest req=(QueryRequest)argument;

			Clause clause=req.getBucketQuery().getClause();
			assertTrue(clause instanceof AllClause);

			return true;
		}
	}


	private QueryResponse getQueryResponse() throws JSONException {
		QueryResponse resp=new QueryResponse();

		resp.setNextPaginationKey(new PaginationKey(""));

		JSONArray objList=new JSONArray();
		JSONObject trans=getEntity();

		trans.put("alipayPartnerID","alipayPartner");
		trans.put("alipayRSAPrivateKey","MIICdQIBADANBgkqhkiG9w0BAQEFAASCAl8wggJbAgEAAoGBAILQ52NUh+tGyBmD/+tNdpsQimrCq5vFRQ1qdDq86v01SYU7X7aArVVliCMjRCHzXlG1wqLIsdj2DCwFpJMvQIUPAn1ESxHjpJRk+BzOq9YHt0RUtCW/Zh/vxXk8KJFwXscHUiZryefEk8lFYqSjtSi1/+/OjEmM7ObrBcnz03nTAgMBAAECgYB9k4yMDdlsQcZUuCIz/2ib8oTtd3LFVdmzxz1MJY5vA3hQLW70AxJhp9XH5VLCRJ2GUrVFDuOrIocMu14A7JtBs8doXShhZ489FJcp2qSwyxM3IHF94+ydganubErBuqiWeDsO1JbSaOqiSsOqOAHUebIwlJcn47FLR9Qnmth+AQJBAO906qsaqyc5dfZpyqpF5YlF5Dc2eRZMSoP0HOtodT60Ck84onem6GmsjSaD9tkSUbOFqjw2you+Mp0XmOlH548CQQCL2otrBnp2r7j62AcyZu9CAedTwUMZsWWCcdk65vypB+aDrrYnBvkbHFD/dtAvgKxRXCaNnWNrmD8j36zQNYd9AkB1eN8TF981O/xB6b5x8B5eTLpb185pgP3hZCW32yQBoeOY38jceDZ2eR858O1tqo4ISx+rsJmHGsTLymLKturDAkAih5oGmbng1fARFwdn3VxTas7nsf7UOCV8+IkO7vsQ7N9wQdG9fw5AMFSbToWGRPnGuL7YFjT22sc0H0S2v5CtAkA9wBqYrlsEKVmQQvdcRS2666CLpf3nqpQci8CsV8nE7cdptfgduo+NyzOj6v48Cyz7c21sKM8Q91F0pObE8CAr");
		trans.put("alipaySecurityKey","alipaySecKey");
		trans.put("alipaySellerID", "alipaySeller");
		trans.put("paypalSandbox","paypaySand");
		trans.put("paypalSandboxSecret","paypaySand");
		trans.put("paypalClientID","paypay");
		trans.put("paypalSecret","paypay");

		trans.put("iapSecurityKey","secKey");

		objList.put(trans);
		resp.setResults(objList);
		return resp;
	}


	private static class ScopeCompare extends ArgumentMatcher<ObjectScope>{

		ObjectScope val=null;
		public ScopeCompare(ObjectScope scope){
			val=scope;
		}

		@Override
		public boolean matches(Object argument) {
			ObjectScope scope=(ObjectScope)argument;

			assertEquals(scope.getAppID(),val.getAppID());
			assertEquals(scope.getUserID(),val.getUserID());

			return true;
		}
	}

	@Test
	public void testConfig() throws BucketNotFoundException, JSONException {


		super.initRequest();


		appContext.su();

		when(bucketClient.query(
				eq(new AccessToken("adminToken")),
				argThat(new ScopeCompare(appContext.getCurrScope())),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(new QueryReq()))).thenReturn(getQueryResponse());



		assertEquals(store.getAlipayConfig().getPartnerID(), "alipayPartner");

		assertEquals(store.getIAPSecurityKey(), "secKey");

		assertEquals(store.getPaypalConfig().getClientID(),"paypay");



	}



	@Test
	public void testSandConfig() throws BucketNotFoundException, JSONException {
		super.initRequestWithSandbox();


		appContext.su();

		when(bucketClient.query(
				eq(new AccessToken("adminToken")),
				argThat(new ScopeCompare(appContext.getCurrScope())),
				eq(new BucketID(BUCKET)),
				eq(BucketType.DEFAULT),
				argThat(new QueryReq()))).thenReturn(getQueryResponse());



		assertEquals(store.getPaypalConfig().getSecret(),"paypaySand");


	}
}
