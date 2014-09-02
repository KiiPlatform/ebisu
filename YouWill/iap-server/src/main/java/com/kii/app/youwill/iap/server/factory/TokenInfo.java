package com.kii.app.youwill.iap.server.factory;

import com.kii.platform.ufp.app.AppID;
import com.kii.platform.ufp.oauth2.AccessToken;

/**
 * Created by ethan on 14-8-6.
 */
public class TokenInfo {


	private String token;

	final AppID appID;

	final boolean isSandbox;

	public TokenInfo(AppID appID,String token,boolean isSandBox) {
		this.appID=appID;
		this.token=token;
		this.isSandbox=isSandBox;
	}

	public AccessToken getAccessToken() {
		return new AccessToken(token);
	}


	public AppID getAppID() {
		return appID;
	}

	public boolean isSandbox(){
		return isSandbox;
	}

	
}
