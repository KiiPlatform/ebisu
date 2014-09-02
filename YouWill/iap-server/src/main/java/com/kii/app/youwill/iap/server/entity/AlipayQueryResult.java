package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.common.CliperSpec;
import com.kii.app.youwill.iap.server.common.IAPUtils;
import org.jdom2.Element;

import java.io.IOException;
import java.io.InputStream;
import java.util.Date;

/**
 * Created by ethan on 14-8-13.
 */
public class AlipayQueryResult {


	private String sign;

	private String signType;

	private boolean isSuccess;

	private String transactionID;

	private String partnerID;

	private TradeStatus tradeStatus;

	private String tradeID;

	private Date gmtClose;

	private String totalFee;

	private Date gmtPayment;

	private Element requestElem;

	private String error;

	private String signReadyStr;

	public AlipayQueryResult(InputStream stream ) throws IOException {


		Element rootElem = IAPUtils.getElementFromStr(stream);

	/*
	  正常输出:
<?xml version="1.0" encoding="utf-8"?>
<alipay>
   <is_success>T</is_success>
   <request>
       <param name="trade_no">2010073000030344</param>
       <param name="service">single_trade_query</param>
       <param name="partner">2088002007018916</param>
   </request>
   <response>
       <trade>
<body>合同催款通知</body> <buyer_email>ltrade008@alitest.com</buyer_email> <buyer_id>2088102002723445
	 */
		String success=rootElem.getChildText("is_success");
		isSuccess="T".equals(success);

		if(!isSuccess){
			error=rootElem.getChildText("error");
			return;
		}

		requestElem=rootElem.getChild("request");
		for(Element param:requestElem.getChildren()){
			if("partner".equals(param.getAttributeValue("name"))) {
				partnerID = param.getText();
			}
		}
		sign=rootElem.getChildText("sign");
		signType= rootElem.getChildText("sign_type");

		Element respElem=rootElem.getChild("response").getChild("trade");

		signReadyStr= CliperSpec.getSignReadyString(respElem);

		transactionID=respElem.getChildText("out_trade_no");
		tradeID=respElem.getChildText("trade_no");
		tradeStatus=TradeStatus.valueOf(respElem.getChildText("trade_status"));

		if(tradeStatus==TradeStatus.TRADE_CLOSED) {
			gmtClose = IAPUtils.convertAlipayDate(respElem.getChildText("gmt_close"));
		}
		if(tradeStatus==TradeStatus.TRADE_FINISHED) {
			gmtPayment = IAPUtils.convertAlipayDate(respElem.getChildText("gmt_payment"));
		}
		totalFee=respElem.getChildText("total_fee");

	}

	public String getSign() {
		return sign;
	}

	public String getSignType() {
		return signType;
	}

	public boolean isSuccess() {
		return isSuccess;
	}

	public String getTransactionID() {
		return transactionID;
	}

	public String getPartnerID() {
		return partnerID;
	}

	public TradeStatus getTradeStatus() {
		return tradeStatus;
	}

	public String getTradeID() {
		return tradeID;
	}

	public Date getGmtClose() {
		return gmtClose;
	}

	public String getTotalFee() {
		return totalFee;
	}

	public Date getGmtPayment() {
		return gmtPayment;
	}

	public Element getRequestElem() {
		return requestElem;
	}

	public String getError() {
		return error;
	}

	public String getSignReadyStr() {
		return signReadyStr;
	}
}
