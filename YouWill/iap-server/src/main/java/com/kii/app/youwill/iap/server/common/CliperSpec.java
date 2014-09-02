package com.kii.app.youwill.iap.server.common;

import org.apache.commons.lang3.StringUtils;
import org.jdom2.Element;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.*;

/**
 * Created by ethan on 14-8-12.
 */
public class CliperSpec {

	public static final String RSA_SIGN_SPEC="SHA1WithRSA";

	public static final String DSA_SIGN_SPEC="SHA1WithDSA";

	public static final String DSA_ENCRYPT_SPEC="DSA";

	public static final String RSA_ENCRYPT_SPEC="RSA";

	public static final String REQUEST_CHARSET="UTF-8";

	public static byte[] toBytes(String str){
		if(str==null){
			return null;
		}
		try{
			return str.getBytes(REQUEST_CHARSET);
		}catch(Exception e){
			throw new IllegalArgumentException(e);
		}
	}

	static public String getSignReadyString(Element elem){

		Map<String,String> map=new HashMap<String,String>();

		for(Element sub:elem.getChildren()){

			String name=sub.getName();
			String key=sub.getText();

			map.put(name,key);

		};
		return getSignReadyString(map);


	}

	static public String getSignReadyString(Map<String,String> paramMap){

		Set<String> keySet=paramMap.keySet();
		keySet.remove("sign_type");
		keySet.remove("sign");

		List<String> list=new ArrayList<String>(keySet);

		Collections.sort(list);
		StringBuffer sb=new StringBuffer();

		for(String key:list){

			String value=paramMap.get(key);
			if(StringUtils.isBlank(value)){
				continue;
			}

			try {
				value= URLDecoder.decode(value, "UTF-8");
				sb.append(key).append("=").append(value).append("&");
			} catch (UnsupportedEncodingException e) {
				throw new IllegalArgumentException(e);
			}
		}

		sb.deleteCharAt(sb.length()-1);

		return sb.toString();

	}

}
