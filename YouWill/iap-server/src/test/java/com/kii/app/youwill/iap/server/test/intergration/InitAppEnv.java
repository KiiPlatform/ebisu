package com.kii.app.youwill.iap.server.test.intergration;

import com.kii.app.youwill.iap.server.entity.ConsumeType;
import com.kii.platform.ufp.app.AppID;
import com.kii.platform.ufp.app.AppKey;
import com.kii.platform.ufp.bucket.BucketID;
import com.kii.platform.ufp.bucket.BucketType;
import com.kii.platform.ufp.bucket.DataType;
import com.kii.platform.ufp.client.HttpParameters;
import com.kii.platform.ufp.client.springrest.RestOperationsProvider;
import com.kii.platform.ufp.client.springrest.impl.apache.HttpComponentsRestTemplateProvider;
import com.kii.platform.ufp.errors.WrongPasswordException;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.scope.ObjectScope;
import com.kii.platform.ufp.ufe.client.http.stateless.AuthClient;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.client.http.stateless.UFEStatelessClient;
import com.kii.platform.ufp.ufe.client.http.stateless.spring.UFEClientImpl;
import com.kii.platform.ufp.ufe.errors.BucketNotFoundException;
import com.kii.platform.ufp.user.LoginName;
import com.kii.platform.ufp.user.Password;
import com.kii.platform.ufp.user.UserID;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.RandomUtils;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Properties;

/**
 * Created by ethan on 14-8-27.
 */
public class InitAppEnv {


	private static final HttpParameters httpParam = new HttpParameters();


	private static final RestOperationsProvider provider=new HttpComponentsRestTemplateProvider();

	private UFEStatelessClient clientFactory;

	private BucketClient bucket;

	private AccessToken token;


	private AppID appID;

	private UserID userID;

	private String fakeUserName;

	private String securityKey;

	private KiiCloudConfigStore configStore;

	public InitAppEnv() throws IOException {

		configStore=new KiiCloudConfigStore();

	}

	public String getBaseHost(){
		return configStore.getIapHost();
	}

	public void init(String fakeUserName) throws URISyntaxException, WrongPasswordException, IOException {


		this.fakeUserName=fakeUserName;

		KiiCloudInfo info=configStore.getAppInfo(fakeUserName);
		UFEStatelessClient clientFactory = new UFEClientImpl(new URI(configStore.getKiiHost()), new AppID(info.appID), new AppKey(info.appKey), httpParam, provider);


		bucket=clientFactory.bucket();

		AuthClient auth=clientFactory.auth();

		AuthClient.UserAuthResponse resp=auth.login(new LoginName(info.user),new Password(info.pwd));
		token=resp.getAccessToken();

		appID=new AppID(info.appID);

		userID=resp.getUserID();

		Properties prop=new Properties();
		String propName=fakeUserName+".fakeconfig.properties";

		prop.load(this.getClass().getResourceAsStream(propName));

		securityKey=prop.getProperty("iapSecurityKey");



	}

	public KiiCloudInfo getKiiCloudInfo(){
		return configStore.getAppInfo(fakeUserName);
	}

	public AccessToken  getToken(){
		return token;
	}

	public String getSecurityKey(){
		return securityKey;
	}



	public void setDataToConfig() throws BucketNotFoundException, JSONException, IOException {


		JSONObject json=new JSONObject();



		Properties prop=new Properties();
		String propName=fakeUserName+".fakeconfig.properties";


		prop.load(this.getClass().getResourceAsStream(propName));


		for(String name:prop.stringPropertyNames()){

			String value=prop.getProperty(name);

			if(value.endsWith(".bin")){
				String context= IOUtils.toString(this.getClass().getResourceAsStream(value));
				json.put(name,context);
			}else {
				json.put(name, value);
			}

		}


		BucketID bucketID=new BucketID("configure");
		ObjectScope scope=new ObjectScope(appID);


		try {
			bucket.deleteBucket(token, scope, bucketID, BucketType.DEFAULT);
		}catch(BucketNotFoundException e){

		}
		bucket.createDataObject(token,scope,bucketID, BucketType.DEFAULT, DataType.APPLICATION_JSON,json);


	}

	public void addDataToProduct(String productID,ConsumeType type) throws JSONException, BucketNotFoundException {

		BucketID bucketID=new BucketID("product");
		ObjectScope scope=new ObjectScope(appID);

		JSONObject json=new JSONObject();
		json.put("productID",productID);
		json.put("price", String.valueOf(RandomUtils.nextFloat(0,1000)));
		json.put("consumeType",type.name());


		bucket.createDataObject(token,scope,bucketID, BucketType.DEFAULT, DataType.APPLICATION_JSON,json);

	}
}
