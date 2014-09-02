package com.kii.app.youwill.iap.server.test.intergration;

import com.kii.app.youwill.iap.server.entity.ConsumeType;
import com.kii.app.youwill.iap.server.entity.OrderStatus;
import com.kii.app.youwill.iap.server.entity.PayType;
import org.apache.commons.lang.StringUtils;
import org.apache.commons.lang3.ArrayUtils;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;

/**
 * Created by ethan on 14-8-27.
 */
public class ClientEntrance {

	public static class Console{

		public void printf(String string) {
			System.out.println(string);
		}

		BufferedReader read=new BufferedReader(new InputStreamReader(System.in));

		public String readLine() {
			try {
				return read.readLine();
			} catch (IOException e) {
				throw new IllegalArgumentException(e);
			}
		}


	}


	public static void  main(String[] argv) throws IOException {


		InitAppEnv appEnv=new InitAppEnv();

		IAPOrderOper  iapOper=new IAPOrderOper(appEnv.getBaseHost());


		String line="";

		Console console=new Console();
		console.printf("welcome kii iap console (input [exit] to quit,[help] to doc)");

		while(true) {
			console.printf("input user name(example:steven):");
			String username = console.readLine();

			try {
				appEnv.init(username);
				appEnv.setDataToConfig();
			} catch (Exception e) {
				e.printStackTrace();
				console.printf("init fail,maybe not found user "+username+" 's config");
				continue;
			}

			iapOper.initAppInfo(appEnv.getKiiCloudInfo(),appEnv.getToken(),appEnv.getSecurityKey());

			console.printf("user "+username+" selected.");
			break;
		}

		String currTranID=null;

		PayType payType=null;

		while((line=console.readLine())!="\n"){

			if(StringUtils.isBlank(line)){
				continue;
			}

			String cmd=StringUtils.trim(line);

			try{
				if(cmd.startsWith("selectuser")){

					String username=StringUtils.substringAfter(cmd," ");
					appEnv.init(username);

					iapOper.initAppInfo(appEnv.getKiiCloudInfo(), appEnv.getToken(), appEnv.getSecurityKey());

					console.printf("user "+username+" selected.");


				}else if("exit".equals(cmd)){
					break;
				}else if("renewconfig".equals(cmd)){
					appEnv.setDataToConfig();
				}else if(cmd.startsWith("addProduct")){

					String[] params= getParams(cmd);

					appEnv.addDataToProduct(params[0], ConsumeType.valueOf(params[1]));
				}else if(cmd.startsWith("startOrder")){


					String[] params= getParams(cmd);
					if(params.length>4||params.length<3){
						console.printf("error param format: productName price paytype [orderID=XXX ]");
					}
					String price=params[1];
					PayType pay=PayType.valueOf(params[2]);
					payType=pay;

					CreateOrderResponse  resp=iapOper.doStartOrder(params[0], pay,price);

					displayPayInfo(console, pay, resp);

					currTranID=resp.getTransactionID();


				}else if(cmd.startsWith("finishCurrOrder")){
					OrderStatus status=null;
					if(payType==PayType.alipay){
						status=iapOper.doFinishAlipayOrder(currTranID);
					}else if(payType==PayType.paypal){
						String param=StringUtils.substringAfter(cmd," ");
						param=StringUtils.trim(param);
						status=iapOper.doFinishOrderInPaypal(currTranID,param);
					}

					console.printf("finish status:"+status.name());


				}else if(cmd.startsWith("finishOrder")){
					Map<String,String> map= getOptionsParams(cmd);
					if(map.size()<1||map.size()>2){
						console.printf("error param format:   [orderID=XXX ,paymentID=XXX]");
					}
					OrderStatus status=null;
					if(map.size()==2){
						status=iapOper.doFinishOrderInPaypal(map.get("orderID"), map.get("paymentID"));
					}else if(map.size()==1){
						status=iapOper.doFinishAlipayOrder(map.get("orderID"));
					}

					console.printf("finish status:"+status.name());


				}else if(cmd.equals("help")){

					console.printf("command list: <require>  [optional] ");
					console.printf("renewconfig:resetting  curr user's configure data from  file [username].fakeconfig.properties ");
					console.printf("selectuser:select spec user's relation info from  file fakeclient.properties,\n " +
							"you maybe need run renewconfig to renew configure data");
					console.printf("addProduct  <productName>  <permanent|consumable|periodical> :add a product to product,can been used by startOrder oper");
					console.printf("startOrder <productName> <price> <alipay|paypal>   [orderID=XXX ] :if not spec price,use product's price,if not spec orderID,use auto-general orderID ");
					console.printf("finishOrder [orderID=XXX,paymentID=XXX]  :if not spec transactionID,use the orderID in before startOrder oper ，if paypal paymode,need input paymentID ");
					console.printf("finishCurrOrder <paymentID> :finishOrder use curr orderID,if paypal mode,need additions paymentID  " );
				}

				console.printf(">>>>>>>>>");

			}catch(Exception e){
				e.printStackTrace();
			}
		}

		System.exit(0);


	}

	private static String[] getParams(String cmd) {
		cmd=StringUtils.trim(cmd);

		if(!StringUtils.contains(cmd," ")){
			return new String[0];
		}
		String[] params= StringUtils.split(cmd, " ");

		return ArrayUtils.remove(params, 0);
//		String lastParam=StringUtils.trim(params[params.length-1]);
//
//		if(lastParam.startsWith("[")&&lastParam.endsWith("]")){
//			return ArrayUtils.remove(params,params.length-1);
//		}else{
//			return params;
//		}
	}

	private static Map<String,String> getOptionsParams(String param) {
		param=StringUtils.trim(param);

		Map<String,String> map=new HashMap<String,String>();

		if(!StringUtils.contains(param,"[")&&!StringUtils.contains(param,"]")){
			return map;
		}
		for(String str:StringUtils.split(StringUtils.substringBetween(param, "[","]"), ",")){
				String key=StringUtils.substringBefore(str,"=");
				String val=StringUtils.substringAfter(str,"=");
				map.put(key,val);
		};
		return map;
	}

	private static void displayPayInfo(Console console, PayType pay, CreateOrderResponse resp) {
		if(pay==PayType.alipay) {
			console.printf("alipay account info:" + resp.getAliPayInfo());
			console.printf("in real env,you can call alipay gateway with upper info");
		}else if(pay==PayType.paypal){
			console.printf("paypal account info:" + resp.getAliPayInfo());
			console.printf("in real env,you can call paypal gateway with upper info");
		}
	}
}
