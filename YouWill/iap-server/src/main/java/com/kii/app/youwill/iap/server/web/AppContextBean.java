package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.factory.TokenInfo;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.platform.ufp.app.AppID;
import com.kii.platform.ufp.app.AppKey;
import com.kii.platform.ufp.app.ParameterName;
import com.kii.platform.ufp.bucket.ObjectScope;
import com.kii.platform.ufp.client.HttpParameters;
import com.kii.platform.ufp.client.springrest.RestOperationsProvider;
import com.kii.platform.ufp.errors.UserNotFoundException;
import com.kii.platform.ufp.oauth2.AccessToken;
import com.kii.platform.ufp.ufe.client.http.stateless.*;
import com.kii.platform.ufp.ufe.client.http.stateless.spring.UFEClientImpl;
import com.kii.platform.ufp.ufe.errors.AppConfigParameterNotFoundException;
import com.kii.platform.ufp.user.UserID;
import org.apache.commons.lang3.StringUtils;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Controller;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import java.net.URI;

/**
 * Created by ethan on 14-7-25.
 */
@Controller
public class AppContextBean implements AppContext {


	@Autowired
	private RestOperationsProvider restOperationsProvider;


	private ServletConfig servletConfig;

	private static final HttpParameters httpParam = new HttpParameters();


	@Value("${iap-server.redirect.kii-site}")
	private String siteUrl;

	@Value("${iap-server.rest.sand-box.prefix}")
	private String sandboxPrefix;

	@Value("${iap-server.kii-parameter.name}")
	private String tokenParameterName;



	private ThreadLocal<TokenInfo> local = new ThreadLocal<TokenInfo>();

	private ThreadLocal<UFEStatelessClient>  factoryLocal=new ThreadLocal<UFEStatelessClient>();

	@Override
	public void bindRequest(HttpServletRequest request){

		AppID appID = new AppID(request.getHeader("x-kii-appid"));
		String appKey = request.getHeader("x-kii-appkey");
		String auth = request.getHeader("Authorization");

		if(StringUtils.isBlank(appID.toString())){
			throw new ServiceException(IAPErrorCode.APPID_IS_NULL);
		}
		if(StringUtils.isBlank(appKey)){
			throw new ServiceException(IAPErrorCode.APPKEY_IS_NULL);
		}
		if(StringUtils.isBlank(auth)){
			throw new ServiceException(IAPErrorCode.TOKEN_IS_NULL);
		}

		String tokenStr = StringUtils.substringAfterLast(auth, " ");


		boolean isSandBox=false;

		String uri=request.getRequestURI();
		if(uri.contains("/sandbox/")){
			isSandBox=true;
		}

		initFactory(appID, new AppKey(appKey));

		local.set(new TokenInfo(appID,tokenStr,isSandBox));

		isAdminLocal.set(false);
		asAppLocal.set(false);
	}



   private  void initFactory(AppID appID,AppKey appKey) {
		try {
			URI baseURI = new URI(siteUrl);

			UFEStatelessClient clientFactory = new UFEClientImpl(baseURI, appID, appKey, httpParam, restOperationsProvider);

			factoryLocal.set(clientFactory);

		} catch (Exception e) {
			throw new IllegalArgumentException(e);
		}
	}


	@Override
	public UserID getUserIDByToken() {

		try {


			return  getUserClient().getUser(new AccessToken(local.get().getAccessToken())).getUserID();

		} catch (UserNotFoundException e) {
			throw new ServiceException(IAPErrorCode.USER_NOT_FOUND);
		}

	}


	private ThreadLocal<Boolean> isAdminLocal=new ThreadLocal<Boolean>();

	private ThreadLocal<Boolean> asAppLocal=new ThreadLocal<Boolean>();


	@Override
	public ObjectScope getCurrScope() {

		if(asAppLocal.get()){
			return new ObjectScope(local.get().getAppID());

		}else {
			UserID id = getUserIDByToken();
			return new ObjectScope(local.get().getAppID(), id);
		}
	}

	@Override
	public void asApp(){
		asAppLocal.set(true);
	}

	@Override
	public void exitApp(){
		asAppLocal.set(false);
	}

	@Override
	public void su(){
		isAdminLocal.set(true);
	}

	@Override
	public void exit(){
		isAdminLocal.set(false);
	}


	@Override
	public TokenInfo getTokenInfo() {

		if(isAdminLocal.get()){
			return getAdminToken(local.get());

		}else {
			return local.get();
		}
	}




	private TokenInfo getAdminToken(TokenInfo tokenInfo){


		try {
			String adminToken=getAppClient().getParameter(tokenInfo.getAccessToken(),tokenInfo.getAppID(),
					new ParameterName(tokenParameterName));

			TokenInfo newToken=new TokenInfo(tokenInfo.getAppID(),adminToken,tokenInfo.isSandbox());

			return newToken;
		} catch (AppConfigParameterNotFoundException e) {
			return tokenInfo;
		}

	}

	@Override
	public boolean isSandBox() {
		return local.get().isSandbox();
	}

	@Override
	public UserClient getUserClient() {
		return factoryLocal.get().user();
	}

	@Override
	public BucketClient getBucketClient() {
		return factoryLocal.get().bucket();
	}

	@Override
	public ACLClient getACLClient() {
		return factoryLocal.get().acl();
	}

	@Override
	public AppClient getAppClient() {return factoryLocal.get().app();}
}
