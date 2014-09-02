package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.aop.KiiScope;
import com.kii.app.youwill.iap.server.aop.ScopeType;
import com.kii.app.youwill.iap.server.dao.CommBucketOperate;
import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.entity.AlipayConfig;
import com.kii.app.youwill.iap.server.entity.ConfigInfo;
import com.kii.app.youwill.iap.server.entity.PaypalConfig;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.BucketID;
import org.apache.commons.codec.binary.Base64;
import org.apache.commons.io.IOUtils;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.io.ResourceLoader;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.io.InputStream;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.spec.X509EncodedKeySpec;
import java.util.List;


/**
 * Created by ethan on 14-7-25.
 */
@Component
@KiiScope(scope= ScopeType.Admin)
public class ConfigInfoStoreImpl implements ConfigInfoStore {


	@Autowired
	private ResourceLoader loader;


	@Autowired
	private AppContext context;


	@Autowired
	private CommBucketOperate bucketDao;


	@Value("${iap-server.pay-server.end-point.domain.paypal.sandbox}")
	private String paypalSandBoxDomain;

	@Value("${iap-server.pay-server.end-point.domain.paypal}")
	private String paypalDomain;


	@Value("${iap-server.pay-server.end-point.public-key.rsa}")
	private String alipayRSAPKPath;

	@Value("${iap-server.pay-server.end-point.public-key.dsa}")
	private String alipayDSAPKPath;


	public static final String BUCKET = "configure";
	private static final BucketID BUCKET_ID = new BucketID(BUCKET);


	private ConfigInfo getConfigInfo() {

		context.su();

		List<JSONObject> list=bucketDao.queryAll(BUCKET);


		return new ConfigInfo(list.get(0));

	}


	@Override
	public PaypalConfig  getPaypalConfig(){

		if(context.isSandBox()){
			return getConfigInfo().getPaypalSandboxInfo();
		}else{
			return getConfigInfo().getPaypalInfo();
		}
	}


	@Override
	public AlipayConfig getAlipayConfig() {
		ConfigInfo info=getConfigInfo();

		AlipayConfig config= info.getAlipayInfo();

		config.setAlipayRSAPublicKey(getRSAPublicKey());

		config.setAlipayDSAPublicKey(getDSAPublicKey());
		return config;
	}

	@Override
	public String getPaypalDomain() {
		if(context.isSandBox()){
			return this.paypalSandBoxDomain;
		}else{
			return this.paypalDomain;
		}
	}

	@Override
	public String getIAPSecurityKey() {
		ConfigInfo info=getConfigInfo();
		return info.getIAPSecurityKey();
	}


	private PublicKey getRSAPublicKey(){

		try {

			InputStream keyfs = loader.getResource("classpath:com/kii/app/youwill/iap/server/"+alipayRSAPKPath).getInputStream();

			String keyContext= IOUtils.toString(keyfs, "ISO-8859-1");

			byte[] encodedKey = Base64.decodeBase64(keyContext);

			X509EncodedKeySpec pubKeySpec = new X509EncodedKeySpec(encodedKey);

			KeyFactory keyFactory = KeyFactory.getInstance("RSA");
			PublicKey pubKey = keyFactory.generatePublic(pubKeySpec);

			return pubKey;

		}catch(IOException e){
			throw new IllegalArgumentException(e);
		} catch (GeneralSecurityException ex) {
			throw new IllegalArgumentException(ex);
		}

	}

	private PublicKey getDSAPublicKey(){

		try {

			InputStream keyfs = loader.getResource("classpath:com/kii/app/youwill/iap/server/"+alipayDSAPKPath).getInputStream();

			String keyContext= IOUtils.toString(keyfs, "ISO-8859-1");

			byte[] encodedKey = Base64.decodeBase64(keyContext);

			X509EncodedKeySpec pubKeySpec = new X509EncodedKeySpec(encodedKey);

			KeyFactory keyFactory = KeyFactory.getInstance("DSA");
			PublicKey pubKey = keyFactory.generatePublic(pubKeySpec);

			return pubKey;

		}catch(IOException e){
			throw new IllegalArgumentException(e);
		} catch (GeneralSecurityException ex) {
			throw new IllegalArgumentException(ex);
		}

	}
}
