package com.kii.app.youwill.iap.server.test.dao;

import com.kii.app.youwill.iap.server.test.BaseTest;
import com.kii.app.youwill.iap.server.common.IAPUtils;
import com.kii.app.youwill.iap.server.entity.AlipayQueryResult;
import com.kii.app.youwill.iap.server.entity.CurrencyType;
import com.kii.app.youwill.iap.server.entity.PaypalQueryResult;
import org.apache.commons.codec.binary.Base64;
import org.apache.commons.io.IOUtils;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.junit.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.core.io.ResourceLoader;

import java.io.IOException;
import java.io.InputStream;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.util.Calendar;
import java.util.Date;

import static org.junit.Assert.assertEquals;

/**
 * Created by ethan on 14-8-20.
 */
public class TestPayHelper  extends BaseTest {


	@Autowired
	private ResourceLoader loader;

	@Test
	public void testAlipayResponse() throws IOException {

		InputStream stream=
				loader.getResource("classpath:com/kii/app/youwill/iap/server/test/DemoAlipayResponse.xml").getInputStream();


		AlipayQueryResult  result=new AlipayQueryResult(stream);

		assertEquals("2010073000030344", result.getTradeID());

		assertEquals("1280463992953",result.getTransactionID());

		assertEquals("1.00",result.getTotalFee());
		assertEquals("TRADE_FINISHED",result.getTradeStatus().name());




	}

	@Test
	public void generKeyPair() throws NoSuchAlgorithmException {

		KeyPairGenerator keyGen = KeyPairGenerator.getInstance("DSA");

		KeyPair keyPair=keyGen.generateKeyPair();

		String pk= Base64.encodeBase64String(keyPair.getPrivate().getEncoded());

		System.out.println(pk);

		System.out.println(Base64.encodeBase64String(keyPair.getPublic().getEncoded()));


	}

//	@Test
	public void testPaypalResponse() throws IOException, JSONException {

		String str= IOUtils.toString(loader.getResource("classpath:com/kii/platform/iap/server/mock/DemoPaypalResponse.json").getInputStream(), "UTF-8");

		JSONObject json=new JSONObject(str);

		PaypalQueryResult  result=new PaypalQueryResult(json);

		assertEquals(result.getState(), PaypalQueryResult.StateType.approved);

		assertEquals(result.getPrice(),"110.54");

		//2013-01-31T19:40:22Z
		Date date =IAPUtils.convertPaypalDate("2013-01-31T19:40:22Z");

		Calendar cal= Calendar.getInstance();

		cal.setTime(result.getUpdateTime());

		assertEquals(cal.get(Calendar.HOUR_OF_DAY),19 );
		assertEquals(cal.get(Calendar.DAY_OF_MONTH),31);
		assertEquals(cal.get(Calendar.MONTH),0);
		assertEquals(cal.get(Calendar.YEAR),2013);

		assertEquals(CurrencyType.USD,result.getCurrency());

		assertEquals(result.getIntent(), PaypalQueryResult.IntentType.sale);
		assertEquals(result.getId(),"PAY-4D099447DD202993VKEFMRJQ");

	}


}
