package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.common.IAPUtils;
import org.apache.commons.lang3.time.DateUtils;
import org.jdom2.Element;
import org.xml.sax.SAXException;

import javax.xml.parsers.ParserConfigurationException;
import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;
import java.util.Date;

/**
* Created by ethan on 14-8-12.
*/
public class NotifyEntity {


	private String transactionID;

	private String sellerID;

	private TradeStatus tradeStatus;

	private String notifyID;

	private Date notifyTime;

	private String price;

	private String alipayTradeID;


	public NotifyEntity(InputStream stream) throws ParserConfigurationException, IOException, SAXException, ParseException {




			Element rootElem = IAPUtils.getElementFromStr(stream);


			this.alipayTradeID=rootElem.getChildText("trade_no");
			this.transactionID = rootElem.getChildText("out_trade_no");
			this.notifyID = rootElem.getChildText("notify_id");

			String dateStr = rootElem.getChildText("notify_time");

			this.notifyTime = DateUtils.parseDate(dateStr, "yyyy-MM-dd HH:mm:ss");

			this.price = rootElem.getChildText("price");

			this.sellerID = rootElem.getChildText("seller_id");

			this.tradeStatus = TradeStatus.valueOf(rootElem.getChildText("trade_status"));


	}


	public String getTransactionID() {
		return transactionID;
	}

	public String getSellerID() {
		return sellerID;
	}

	public TradeStatus getTradeStatus() {
		return tradeStatus;
	}

	public String getNotifyID() {
		return notifyID;
	}

	public Date getNotifyTime() {
		return notifyTime;
	}

	public String getPrice() {
		return price;
	}

	public String getAlipayTradeID() {
		return alipayTradeID;
	}


}
