package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.platform.ufp.app.AppID;
import com.kii.platform.ufp.bucket.ObjectScope;
import com.kii.platform.ufp.errors.UserNotFoundException;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.oauth2.ClientID;
import com.kii.platform.ufp.oauth2.ClientSecret;
import com.kii.platform.ufp.ufe.client.http.stateless.AuthClient;
import com.kii.platform.ufp.ufe.client.http.stateless.UserClient;
import com.kii.platform.ufp.user.UserID;
import org.apache.commons.lang3.StringUtils;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Controller;

import javax.annotation.PostConstruct;
import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;

/**
 * Created by ethan on 14-7-25.
 */
@Controller
public class AppContext {



	private ServletConfig servletConfig;


	@Value("${iap-server.rest.sand-box.prefix}")
	private String sandboxPrefix;


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


	public AppID getAppID(){
		return appID;
	}


	private ClientID clientID;
	@Value("${iap-server.kii.client-id}")
	public void setClientID(String id){
		this.clientID=new ClientID(id);

	}

	private ClientSecret secret;
	@Value("${iap-server.kii.client-secret}")
	public void setClientSecret(String sec){
		secret=new ClientSecret(sec);
	}

	private AccessToken adminToken;

	@Autowired
	private AuthClient auth;

	@Autowired
	private UserClient user;


	@PostConstruct
	public void initAdminToken(){

		adminToken=auth.loginAsAppAdmin(appID,clientID,secret).getAccessToken();

	}

    private ThreadLocal<UserID>  asUserLocal=new ThreadLocal<UserID>();

    public void sudo(UserID userID) {
        asUserLocal.set(userID);
    }


	public ObjectScope getCurrScope() {

       UserID id=asUserLocal.get();

       if(id==null){
            return new ObjectScope(appID);
       }else {
            return new ObjectScope(appID, id);
       }
	}


	public void exitScope(){

        asUserLocal.remove();
	}



	public AccessToken getAccessToken() {

		return adminToken;
	}

	public boolean isSandBox() {
		return false;
	}




}
