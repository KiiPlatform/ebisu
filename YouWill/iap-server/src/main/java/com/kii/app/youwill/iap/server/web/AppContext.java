package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.factory.TokenInfo;
import com.kii.platform.ufp.bucket.ObjectScope;
import com.kii.platform.ufp.ufe.client.http.stateless.ACLClient;
import com.kii.platform.ufp.ufe.client.http.stateless.AppClient;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.client.http.stateless.UserClient;
import com.kii.platform.ufp.user.UserID;

import javax.servlet.http.HttpServletRequest;

/**
 * Created by ethan on 14-8-19.
 */
public interface AppContext {
	UserID getUserIDByToken();

	ObjectScope getCurrScope();

	void su();

	void exit();


	public void asApp();


	public void exitApp();

	TokenInfo getTokenInfo();

	boolean isSandBox();


	UserClient getUserClient();

	BucketClient getBucketClient();

	ACLClient getACLClient();

	AppClient getAppClient();

	void bindRequest(HttpServletRequest request);

}
