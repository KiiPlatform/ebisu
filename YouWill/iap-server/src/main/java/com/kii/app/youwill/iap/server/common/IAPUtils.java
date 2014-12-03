package com.kii.app.youwill.iap.server.common;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.apache.commons.lang3.StringUtils;
import org.apache.commons.lang3.time.DateUtils;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.input.DOMBuilder;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import java.io.InputStream;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;

/**
 * Created by ethan on 14-8-13.
 */
public class IAPUtils {


	public static Date convertAlipayDate(String dateStr){
		try {
			return 	 DateUtils.parseDate(dateStr, "yyyy-MM-dd HH:mm:ss");
		} catch (ParseException e) {

			throw new ServiceException(IAPErrorCode.ALIPAY_SERVICE_INVALID);
		}

	}

	public static Date convertMMPayDate(String dateStr) {
		try {
			return 	 DateUtils.parseDate(dateStr, "yyyyMMddHHmmss");
		} catch (ParseException e) {
			throw new ServiceException(IAPErrorCode.ALIPAY_SERVICE_INVALID);
		}
	}


	public static String parseAlipayDate(Date date){
		SimpleDateFormat format=new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
		return 	 format.format(date);

	}

	public static Element getElementFromStr(InputStream input)  {
		try {
			DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
			factory.setNamespaceAware(true);
			DocumentBuilder dombuilder = factory.newDocumentBuilder();
			org.w3c.dom.Document w3cDocument = dombuilder.parse(input);
			DOMBuilder jdomBuilder = new DOMBuilder();
			Document jdomDocument = jdomBuilder.build(w3cDocument);

			return jdomDocument.getRootElement();
		}catch(Exception e){
			throw new ServiceException(IAPErrorCode.ALIPAY_SERVICE_INVALID);
		}
	}

	public static Document newDocument(){

		try {
			DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
			factory.setNamespaceAware(true);
			DocumentBuilder dombuilder = factory.newDocumentBuilder();
			org.w3c.dom.Document w3cDocument=dombuilder.newDocument();

			DOMBuilder jdomBuilder = new DOMBuilder();
			Document jdomDocument = jdomBuilder.build(w3cDocument);


			return jdomDocument;
		}catch(Exception e){
			throw new ServiceException(IAPErrorCode.ALIPAY_SERVICE_INVALID);
		}

	}

	public static Date convertPaypalDate(String dateStr) {

		try {

			String reveStr=StringUtils.reverse(dateStr);
			int idx=StringUtils.indexOfAny(reveStr,'Z','+','-');

			idx=dateStr.length()-idx-1;
			String subDateStr=StringUtils.substring(dateStr,0,idx);

			Date date=null;
			if(subDateStr.contains(".")){
				date=DateUtils.parseDate(subDateStr, "yyyy-MM-dd'T'HH:mm:ss.SSS");
			}else{
				date=DateUtils.parseDate(subDateStr, "yyyy-MM-dd'T'HH:mm:ss");
			}

			if(idx<dateStr.length()-1) {
				String timeArea = StringUtils.substring(dateStr, idx + 1);
				String hh=StringUtils.substringBefore(timeArea,":");
				String mm=StringUtils.substringAfter(timeArea,":");

			}

			return 	 date;
		} catch (ParseException e) {
			e.printStackTrace();
			throw new ServiceException(IAPErrorCode.PAYPAL_SERVICE_INVALID);
		}
	}

	public static String parsePaypalDate(Date date) {

		//2013-01-31T19:40:22Z
		DateFormat format=new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

		String str=format.format(date);

		Calendar cal= Calendar.getInstance();
		cal.setTime(date);

		int msec=cal.get(Calendar.MILLISECOND);
		if(msec != 0){
			str+="."+msec;
		}

		int zone=cal.get(Calendar.ZONE_OFFSET);

		if(zone!=0){
			str+=zone;
		}else{
			str+="Z";
		}

		return str;

	}
}
