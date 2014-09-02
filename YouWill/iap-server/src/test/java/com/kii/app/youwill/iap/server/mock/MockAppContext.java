package com.kii.app.youwill.iap.server.mock;

import com.kii.app.youwill.iap.server.web.AppContextBean;
import com.kii.platform.ufp.ufe.client.http.stateless.ACLClient;
import com.kii.platform.ufp.ufe.client.http.stateless.AppClient;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.client.http.stateless.UserClient;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

/**
 * Created by ethan on 14-8-19.
 */
@Component
public class MockAppContext extends AppContextBean {

//
	@Autowired
	private UserClient userClient;

	@Autowired
	private BucketClient bucketClient;

	@Autowired
	private ACLClient aclClient;

	@Autowired
	private AppClient appClient;

	@Override
	public UserClient getUserClient() {

		return userClient;
	}
	@Override
	public BucketClient getBucketClient() {

		return bucketClient;
	}
	@Override
	public ACLClient getACLClient() {

		return aclClient;
	}

	@Override
	public AppClient getAppClient(){
		return appClient;
	}


}
