package com.kii.app.youwill.iap.server.test.intergration;

import org.apache.commons.lang3.StringUtils;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

/**
 * Created by ethan on 14-8-27.
 */
public class KiiCloudConfigStore {


	private Map<String,KiiCloudInfo> infoMap=new HashMap<String,KiiCloudInfo>();

	private String kiiHost=null;

	private String  iapHost=null;

	public KiiCloudConfigStore() throws IOException {

		Properties prop=new Properties();

		prop.load(this.getClass().getResourceAsStream("fakeclient.properties"));

		kiiHost=prop.getProperty("client.iap.fake-base");

		iapHost=prop.getProperty("client.iap.site");

		for(String name:prop.stringPropertyNames()){


			if(!name.startsWith("client.kii.")){

				continue;
			}


			String[] valArray= StringUtils.split(name, ".");

			String userName=valArray[2];
			String subKey=valArray[3];

			if(!infoMap.containsKey(userName)){
				infoMap.put(userName,new KiiCloudInfo());
			}

			String value=prop.getProperty(name);
			KiiCloudInfo info=infoMap.get(userName);

			if(subKey.equals("app-id")) {
				info.appID = value;
			}else if(subKey.equals("app-key")){
				info.appKey=value;
			}else if(subKey.equals("user")){
				info.user=value;
			}else if(subKey.equals("pwd")){
				info.pwd=value;
			}

		}

	}

	public String getKiiHost(){
		return kiiHost;
	}

	public String getIapHost(){
		return iapHost;
	}

	public KiiCloudInfo getAppInfo(String name){
		return infoMap.get(name);
	}
}
