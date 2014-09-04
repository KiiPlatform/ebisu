package com.kii.app.youwill.iap.server.factory;

import com.kii.platform.ufp.app.AppID;
import com.kii.platform.ufp.app.AppKey;
import com.kii.platform.ufp.client.HttpParameters;
import com.kii.platform.ufp.client.springrest.RestOperationsProvider;
import com.kii.platform.ufp.ufe.client.http.stateless.*;
import com.kii.platform.ufp.ufe.client.http.stateless.spring.UFEClientImpl;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import javax.annotation.PostConstruct;
import java.net.URI;
import java.net.URISyntaxException;

/**
 * Created by ethan on 14-9-2.
 */
@Configuration
public class KiiAppFactory {


	@Autowired
	private RestOperationsProvider restOperationsProvider;

	private static final HttpParameters httpParam = new HttpParameters();

/*
iap-server.kii.site=https://api.kii.com/api
iap-server.kii.app-id=
iap-server.kii.app-key=
iap-server.kii.client-id=
iap-server.kii.client-secret=
 */
	private AppID appID;
	@Value("${iap-server.kii.app-id}")
	public void setAppID(String id){
		appID=new AppID(id);
	}

	private AppKey appKey;
	@Value("${iap-server.kii.app-key}")
	public void setAppKey(String key){
		appKey=new AppKey(key);
	}


	private String siteUrl;

	@Value("${iap-server.kii.site}")

	public void setSiteUrl(String url){
		siteUrl=url+"/api";
	}

	private UFEStatelessClient clientFactory;


	@PostConstruct
	public void initFactory() throws URISyntaxException {

		URI baseURI = new URI(siteUrl);

		clientFactory = new UFEClientImpl(baseURI, appID
				, appKey, httpParam, restOperationsProvider);

	}

	@Bean
	public UserClient getUserClient() {
		return clientFactory.user();
	}

	@Bean
	public BucketClient getBucketClient() {
		return clientFactory.bucket();
	}

	@Bean
	public ACLClient getACLClient() {
		return clientFactory.acl();
	}

	@Bean
	public AuthClient getAuthClient() {return clientFactory.auth();}
}
