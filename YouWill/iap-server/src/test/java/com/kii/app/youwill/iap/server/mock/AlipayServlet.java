package com.kii.app.youwill.iap.server.mock;

import com.kii.app.youwill.iap.server.common.CliperSpec;
import com.kii.app.youwill.iap.server.common.IAPUtils;
import com.kii.app.youwill.iap.server.dao.impl.AlipayErrorCode;
import com.kii.app.youwill.iap.server.entity.TradeStatus;
import org.apache.commons.codec.binary.Base64;
import org.apache.commons.codec.digest.DigestUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.time.DateUtils;
import org.apache.commons.lang3.StringUtils;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.output.XMLOutputter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.security.*;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.*;

/**
 * Created by ethan on 14-8-25.
 */
public class AlipayServlet extends  HttpServlet {

	private Logger log= LoggerFactory.getLogger(AlipayServlet.class);



	private PrivateKey rsaPriKey;

	private PrivateKey dsaPriKey;

	private PublicKey userRSAPubKey;

	private PublicKey userDSAPubKey;
	private String securityKey;



	public void initServlet() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {


		Properties  prop=new Properties();
		InputStream stream=AlipayServlet.class.getResourceAsStream("alipay_servlet.properties");

		prop.load(stream);

		securityKey=prop.getProperty("alipay.security-key");

		KeyFactory keyFactory = KeyFactory.getInstance("RSA");

		PKCS8EncodedKeySpec priKeySpec = getKeySpec(prop.getProperty("alipay.rsa.private-key.file"));
		rsaPriKey = keyFactory.generatePrivate(priKeySpec);

		X509EncodedKeySpec pubKeySpec=getPubKeySpec(prop.getProperty("user.rsa.public-key.file"));
		userRSAPubKey=keyFactory.generatePublic(pubKeySpec);

		keyFactory = KeyFactory.getInstance("DSA");


		priKeySpec = getKeySpec(prop.getProperty("alipay.dsa.private-key.file"));
		dsaPriKey = keyFactory.generatePrivate(priKeySpec);

		pubKeySpec=getPubKeySpec(prop.getProperty("user.dsa.public-key.file"));
		userDSAPubKey=keyFactory.generatePublic(pubKeySpec);

	}

	private PKCS8EncodedKeySpec getKeySpec(String fileName) throws IOException {
		InputStream stream=this.getClass().getResourceAsStream(fileName);
		String keyContext= IOUtils.toString(stream, "ISO-8859-1");

		byte[] encodedKey = Base64.decodeBase64(keyContext);

		return new PKCS8EncodedKeySpec(encodedKey);
	}

	private X509EncodedKeySpec getPubKeySpec(String fileName) throws IOException {
		InputStream stream=this.getClass().getResourceAsStream(fileName);
		String keyContext= IOUtils.toString(stream, "ISO-8859-1");

		byte[] encodedKey = Base64.decodeBase64(keyContext);

		return new X509EncodedKeySpec(encodedKey);
	}


//	String[] paramArray={"_input_charset","out_trade_no","partner","service","trade_no"};
	@Override
	public void doPost(HttpServletRequest request,HttpServletResponse response)
			throws ServletException, IOException {

		/*

https://mapi.alipay.com/gateway.do?
service=single_trade_query
&sign=d8ed9f0 15214e7cd59bfadb6c945a87b
&trade_no=2010121502730740
&partner=208800 1159940003
&out_trade_no=2109095506028810
&sign_type=MD5
		 */

		try{


			Map<String,String> paramMap=new HashMap<String,String>();

			for(Map.Entry<String,String[]> entry:request.getParameterMap().entrySet() ){
				String name=entry.getKey();

				paramMap.put(name,entry.getValue()[0]);
			}
			String respXml=doService(paramMap);

			fillResponse(respXml,response);

		}catch(AlipayException e){
			fillResponse(getErrorResp(e.code), response);
			return;
		}

	}

	private String doService(Map<String,String> paramMap) throws AlipayException{



		if(!"single_trade_query".equals(paramMap.get("service"))){

			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SERVICE);

		}

		String sign=paramMap.get("sign");

		if(StringUtils.isBlank(sign)){
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SIGN);
		}

		String signType=paramMap.get("sign_type");


		String charset=paramMap.get("_input_charset");
		if(StringUtils.isBlank(charset)){
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SIGN_TYPE);
		}


		verifySign(sign, signType, paramMap, charset);


		Map<String,String> respMap=getRespMap(paramMap.get("trade_no"),paramMap.get("out_trade_no"),paramMap.get("partner"));

		String respSign = generSign(signType, charset, respMap);

		Document docs=IAPUtils.newDocument();

		Element root=new Element("alipay");

		root.addContent(new Element("is_success").setText("T"));

		Element req=new Element("request");

		for(String key:new String[]{"out_trade_no","service","partner"}) {
			Element elem = new Element("param");
			elem.setAttribute("name", key);
			elem.setText(paramMap.get(key));
			req.addContent(elem);
		}

		root.addContent(req);

		root.addContent(getRespElement(respMap));

		root.addContent(new Element("sign").setText(respSign));
		root.addContent(new Element("sign_type").setText(signType));

		XMLOutputter serializer = new XMLOutputter();

		String respContext= serializer.outputString(root);

		return respContext;

	}

	private String generSign(String signType, String charset, Map<String, String> respMap) {

		List<String>   keyList=new ArrayList<String>(respMap.keySet());

		Collections.sort(keyList);

		StringBuffer str=new StringBuffer();
		for(String key: keyList ){
			str.append(key).append("=").append(respMap.get(key)).append("&");
		}
		str.deleteCharAt(str.length()-1);



		String respSign=null;
		try{

			if("MD5".equals(signType)){

				respSign=computeSignByMD5(str.toString(),charset);

			}else if("RSA".equals(signType)){

				respSign=computeSignByRSA(str.toString(),charset);
			}else if("DSA".equals(signType)){
				respSign=computeSignByDSA(str.toString(),charset);
			}else{
				throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SIGN_TYPE);
			}

		}catch(UnsupportedEncodingException e){
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_CHARSET);
		} catch (NoSuchAlgorithmException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		} catch (SignatureException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		} catch (NoSuchProviderException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		} catch (InvalidKeyException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		}
		return respSign;
	}

	private void verifySign(String sign, String signType, Map<String,String> paramMap,String charset) {

		StringBuffer buf=new StringBuffer();

		boolean isParam=false;
		Set<String> keySet=paramMap.keySet();
		keySet.remove("sign");
		keySet.remove("sign_type");

		List<String> sortKey=new ArrayList<String>(keySet);
		Collections.sort(sortKey);

		if(!keySet.contains("out_trade_no")&&!keySet.contains("trade_no")){
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ARGUMENT);
		}


		for(String param:sortKey){
			String value=paramMap.get(param);


			if(StringUtils.isBlank(value)){
				throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ARGUMENT);
			}

			buf.append(param).append("=").append(value).append("&");
		}
		buf.deleteCharAt(buf.length()-1);



		try{

			if("MD5".equals(signType)){

				if(!sign.equals(computeSignByMD5(buf.toString(),charset)) ){
					throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SIGN);
				};


			}else if("RSA".equals(signType)){

				if(!verifySignByRSA(buf.toString(),charset,sign) ){
					throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SIGN);
				};
			}else if("DSA".equals(signType)){
				if(!verifySignByDSA(buf.toString(),charset,sign) ){
					throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SIGN);
				};

			}else{
				throw AlipayException.getException(AlipayErrorCode.ILLEGAL_SIGN_TYPE);
			}

		}catch(UnsupportedEncodingException e){
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_CHARSET);
		} catch (NoSuchAlgorithmException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		} catch (SignatureException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		} catch (NoSuchProviderException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		} catch (InvalidKeyException e) {
			throw AlipayException.getException(AlipayErrorCode.ILLEGAL_ENCRYPT);
		}
	}

	private Map<String,String> getRespMap(String outTradeNo,String tradeNo,String partNo){

		Map<String,String> respMap=new HashMap<String,String>();

		respMap.put("body","测试");
		respMap.put("buyer_email","buyer@foo.com");
		respMap.put("buyer_id","2088002007274897");

		Date date=new Date();
		Date createDate=DateUtils.addDays(date,-10);
		Date modifyDate=DateUtils.addDays(date,-8);
		Date payDate=DateUtils.addDays(date,-5);

		respMap.put("gmt_create",IAPUtils.parseAlipayDate(createDate));
		respMap.put("gmt_last_modifyied_time",IAPUtils.parseAlipayDate(modifyDate));
		respMap.put("gmt_payment",IAPUtils.parseAlipayDate(payDate));

		respMap.put("is_total_fee_adjust","F");
		if(StringUtils.isNotBlank(outTradeNo)) {
			respMap.put("out_trade_no", outTradeNo);
		}
		respMap.put("price","12.34");
		respMap.put("quantity","8");
		respMap.put("seller_email","seller@foo.com");
		respMap.put("seller_id","test@foo.com");
		respMap.put("subject","测试用商品");
		respMap.put("total_fee","98.72");
		if(StringUtils.isNoneBlank(tradeNo)) {
			respMap.put("trade_no", tradeNo);
		}else{
			respMap.put("trade_no",String.valueOf(date.getTime()));
		}
		respMap.put("trade_status", TradeStatus.TRADE_SUCCESS.name());


		/*

<body>合同催款通知</body>
<buyer_email>ltrade008@alitest.com</buyer_email>
<buyer_id>2088102002723445</buyer_id>
<discount>0.00</discount>
<gmt_create>2010-07-30 12:26:33</gmt_create>
<gmt_last_modified_time>2010-07-30 12:30:29 </gmt_last_modified_time>
<gmt_payment>2010-07-30 12:30:29</gmt_payment>
<is_total_fee_adjust>F</is_total_fee_adjust>
<out_trade_no>1280463992953</out_trade_no> <payment_type>1</payment_type>
<price>1.00</price>
<quantity>1</quantity>
<seller_email>chao.chenc1@alipay.com</seller_email>
<seller_id>2088002007018916</seller_id>
<subject>合同催款通知</subject>
<total_fee>1.00</total_fee>
<trade_no>2010073000030344</trade_no>
<trade_status>TRADE_FINISHED</trade_status>
<use_coupon>F</use_coupon>
		 */

		return respMap;


	}

	private Element getRespElement(Map<String,String> respMap){

		 Element elem=new Element("trade");

		for(Map.Entry<String,String> entry:respMap.entrySet()){

			String key=entry.getKey();
			String value=entry.getValue();

			Element sub=new Element(key);
			sub.setText(value);

			elem.addContent(sub);
		}

		Element respElem=new Element("response");
		respElem.addContent(elem);

		return respElem;

	}

	private String computeSignByMD5(String str,String charset) throws UnsupportedEncodingException {

		str+=securityKey;
		return DigestUtils.md5Hex(str.getBytes(charset));
	}

	private boolean verifySignByRSA(String str,String charset,String sign) throws UnsupportedEncodingException, NoSuchAlgorithmException, InvalidKeyException, SignatureException {


		Signature sig = Signature.getInstance(CliperSpec.RSA_SIGN_SPEC);

		sig.initVerify(userRSAPubKey);

		sig.update(str.getBytes(charset));

		return sig.verify(Base64.decodeBase64(sign));
	}

	private String computeSignByRSA(String str,String charset) throws UnsupportedEncodingException, NoSuchAlgorithmException, InvalidKeyException, SignatureException {


		Signature sig = Signature.getInstance(CliperSpec.RSA_SIGN_SPEC);

		sig.initSign(rsaPriKey);

		sig.update(str.getBytes(charset));

		return Base64.encodeBase64String(sig.sign());

	}

	private boolean verifySignByDSA(String str,String charset,String sign) throws UnsupportedEncodingException, NoSuchProviderException, NoSuchAlgorithmException, InvalidKeyException, SignatureException {

		Signature sig  = Signature.getInstance(CliperSpec.DSA_SIGN_SPEC);

		sig.initVerify(userDSAPubKey);

		sig.update(str.getBytes(charset));

		return sig.verify(Base64.decodeBase64(sign));
	}

	private String computeSignByDSA(String str,String charset) throws UnsupportedEncodingException, NoSuchProviderException, NoSuchAlgorithmException, InvalidKeyException, SignatureException {

		Signature sig  = Signature.getInstance(CliperSpec.DSA_SIGN_SPEC);

		sig.initSign(dsaPriKey);

		sig.update(str.getBytes(charset));

		return Base64.encodeBase64String(sig.sign());
	}

	private void fillResponse(String context,HttpServletResponse response) throws IOException {

		OutputStream  stream=response.getOutputStream();

		IOUtils.write(context,stream,"UTF-8");

	}

	private String getErrorResp(AlipayErrorCode code){
		/*
		alipay>
   <is_success>F</is_success>
   <error>TRADE_IS_NOT_EXIST</error>
</alipay>
		 */


		Document docs= IAPUtils.newDocument();
		Element elem=new Element("alipay");
		Element isSuc=new Element("is_success").setText("F");

		Element error=new Element("error").setText(code.name());

		elem.addContent(isSuc);
		elem.addContent(error);

		docs.setContent(elem);

		XMLOutputter serializer = new XMLOutputter();

		return serializer.outputString(docs);

	}

	private static class AlipayException extends RuntimeException{

		private AlipayErrorCode code;

		public AlipayException(AlipayErrorCode code){
			this.code=code;
		}

		public static AlipayException getException(AlipayErrorCode code){
			return new AlipayException(code);
		}
	}




}
