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

	protected ThreadLocal<TokenInfo> local = new ThreadLocal<TokenInfo>();

	@PostConstruct
	public void initAdminToken(){

		adminToken=auth.loginAsAppAdmin(appID,clientID,secret).getAccessToken();

	}


	public void bindRequest(HttpServletRequest request){

		String auth = request.getHeader("Authorization");

		if(StringUtils.isBlank(auth)){
			throw new ServiceException(IAPErrorCode.TOKEN_IS_NULL);
		}

		String tokenStr = StringUtils.substringAfterLast(auth, " ");


		boolean isSandBox=false;

		String uri=request.getRequestURI();
		if(uri.contains("/sandbox/")){
			isSandBox=true;
		}


		local.set(new TokenInfo(tokenStr,isSandBox));

		isAdminLocal.set(false);
		asAppLocal.set(false);
	}


	public UserID getCurrUserID() {

		try {

			return  user.getUser(new AccessToken(local.get().getAccessToken())).getUserID();

		} catch (UserNotFoundException e) {
			throw new ServiceException(IAPErrorCode.USER_NOT_FOUND);
		}

	}


	private ThreadLocal<Boolean> isAdminLocal=new ThreadLocal<Boolean>();

	private ThreadLocal<Boolean> asAppLocal=new ThreadLocal<Boolean>();


	public ObjectScope getCurrScope() {

		if(asAppLocal.get()){
			return new ObjectScope(appID);

		}else {
			UserID id = getCurrUserID();
			return new ObjectScope(appID, id);
		}
	}

	public void asApp(){
		asAppLocal.set(true);
	}

	public void exitApp(){
		asAppLocal.set(false);
	}

	public void su(){
		isAdminLocal.set(true);
	}

	public void exit(){
		isAdminLocal.set(false);
	}


	public AccessToken getAccessToken() {

		if(isAdminLocal.get()){
			return adminToken;

		}else {
			return local.get().getAccessToken();
		}
	}

	public boolean isSandBox() {
		return local.get().isSandbox();
	}

	protected class TokenInfo {


		private String token;


		final boolean isSandbox;

		public TokenInfo(String token,boolean isSandBox) {
			this.token=token;
			this.isSandbox=isSandBox;
		}

		public AccessToken getAccessToken() {
			return new AccessToken(token);
		}



		public boolean isSandbox(){
			return isSandbox;
		}


	}

}
