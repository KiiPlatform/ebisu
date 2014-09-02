package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.common.CliperSpec;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.apache.commons.codec.binary.Base64;
import org.apache.commons.codec.binary.Hex;
import org.apache.commons.codec.digest.DigestUtils;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import javax.crypto.Cipher;
import java.io.*;
import java.security.*;
import java.security.spec.KeySpec;
import java.security.spec.PKCS8EncodedKeySpec;

/**
 * Created by ethan on 14-8-12.
 */
public class AlipayConfig {


	private PrivateKey rsaPrivateKey;

	private PrivateKey dsaPrivateKey;

	private String partnerID;

//	private String sellerID;

	private String securityKey;

	private PublicKey alipayRSAPublicKey;

	private PublicKey alipayDSAPublicKey;


	public void setAlipayRSAPublicKey(PublicKey key){
		this.alipayRSAPublicKey=key;
	}

	public void setAlipayDSAPublicKey(PublicKey key){
		this.alipayDSAPublicKey=key;
	}

	private JSONObject  alipayJson;

	public JSONObject getAlipayJson(){

		return alipayJson;
	}


	public AlipayConfig(JSONObject obj){
		try {


			partnerID = obj.getString("alipayPartnerID");

			if(obj.has("alipayRSAPrivateKey")){
				byte[] keyBytes = Base64.decodeBase64(obj.getString("alipayRSAPrivateKey"));

				KeySpec spec = new PKCS8EncodedKeySpec(keyBytes);
				try {
					KeyFactory kf = KeyFactory.getInstance("RSA");
					rsaPrivateKey = kf.generatePrivate(spec);
				}catch(GeneralSecurityException e){
					throw new ServiceException(IAPErrorCode.ALIPAY_CONFIG_MISS);
				}

			}
			securityKey = obj.getString("alipaySecurityKey");

			alipayJson=new JSONObject(obj,new String[]{"alipaySellerID","alipayPartnerID",
					"alipayRSAPrivateKey","alipaySecurityKey"});



		}catch(JSONException e){
			throw new ServiceException(IAPErrorCode.ALIPAY_CONFIG_MISS);
		}

	}



	public String signWithMD5(String orginStr){
		String fullStr=orginStr+this.securityKey;

		byte[] fullBytes=CliperSpec.toBytes(fullStr);

		return DigestUtils.md5Hex(fullBytes);

	}

	public String signWithRSA(String orginStr){


		if(rsaPrivateKey==null){
			throw new ServiceException(IAPErrorCode.ALIPAY_CONFIG_MISS);
		}

		try {

			byte[] bytes=CliperSpec.toBytes(orginStr);

			Signature signature = Signature.getInstance(CliperSpec.RSA_SIGN_SPEC);

			signature.initSign(this.rsaPrivateKey);

			signature.update(bytes);

			byte[] sigBytes = signature.sign();

			return Hex.encodeHexString(sigBytes);

		}  catch (GeneralSecurityException e) {
			throw new IllegalArgumentException(e);
		}
	}


	public boolean verifyWithMD5(String orginStr,String md5Sign){

			String fullStr=orginStr+securityKey;

			String sign= DigestUtils.md5Hex(CliperSpec.toBytes(fullStr));
			return md5Sign.equals(sign);


	}

	public String decrypt(String encrypt){


		if(rsaPrivateKey==null){
			throw new ServiceException(IAPErrorCode.ALIPAY_CONFIG_MISS);
		}

		try {
			Cipher rsa = Cipher.getInstance(CliperSpec.RSA_ENCRYPT_SPEC);

			rsa.init(Cipher.DECRYPT_MODE,rsaPrivateKey);


			InputStream ins = new ByteArrayInputStream(Base64.decodeBase64(encrypt));

			ByteArrayOutputStream writer = new ByteArrayOutputStream();

			byte[] buf = new byte[128];
			int len;

			while ((len = ins.read(buf)) != -1) {
				byte[] block =null;
				if(len==buf.length){
					block=buf;
				}else {
					block=new byte[len];
					System.arraycopy(buf, 0, block, 0, block.length);
				}
				writer.write(rsa.doFinal(block));
			}

			return new String(writer.toByteArray(), "UTF-8");

		} catch (IOException e) {
			throw new IllegalArgumentException(e);
		} catch (GeneralSecurityException e) {
			throw new IllegalArgumentException(e);
		}


	}


	public boolean verifyWithDSA(String  orginStr,String sign){

		try {


			Signature sig = Signature.getInstance(CliperSpec.DSA_SIGN_SPEC);

			sig.initVerify(alipayDSAPublicKey);

			sig.update(orginStr.getBytes("UTF-8"));

			return sig.verify(Base64.decodeBase64(sign));


		} catch (GeneralSecurityException e) {
			throw new IllegalArgumentException(e);
		}catch(UnsupportedEncodingException ex){
			throw new IllegalArgumentException(ex);
		}

	}


	public boolean verifyWithRSA(String  orginStr,String sign){

		try {


			Signature sig = Signature.getInstance(CliperSpec.RSA_SIGN_SPEC);

			sig.initVerify(alipayRSAPublicKey);

			sig.update(orginStr.getBytes("UTF-8"));

			return sig.verify(Base64.decodeBase64(sign));


		} catch (GeneralSecurityException e) {
			throw new IllegalArgumentException(e);
		}catch(UnsupportedEncodingException ex){
			throw new IllegalArgumentException(ex);
		}

	}




	public String getPartnerID() {
		return partnerID;
	}

//	public String getSellerID() {
//		return sellerID;
//	}
}
