package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.common.StringTemplate;
import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.entity.AlipayConfig;
import com.kii.app.youwill.iap.server.entity.NotifyEntity;
import com.kii.app.youwill.iap.server.entity.Transaction;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RequestParam;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.ByteArrayInputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.*;

/**
 * Created by ethan on 14-8-11.
 */
public class AlipayCallbackController {


	@Autowired
	private ConfigInfoStore store;
	@Autowired
	private TransactionDao transactionDao;


	@RequestMappingProp("alipay-callback")
	public ResponseEntity<String> callback(@RequestParam Map<String,String> allRequestParams,HttpServletResponse response){

/*
http://商户自定义地址 /notify_url.php?
trade_no=2014040311001004370000361525&
out_trade_no=36 18810634349901&
discount=-5&
payment_type=1&
subject=iphone%E6%89%8B %E6%9C%BA&
body=Hello&
price=10.00&
quantity=1&
total_fee=10.00&
trade_st atus=TRADE_FINISHED&
refund_status=REFUND_SUCCESS&
seller_email=ch ao.chenc1%40alipay.com&
seller_id=2088002007018916&
buyer_id=208800200 7013600&
buyer_email=13758698870&
gmt_create=2008-10-22+20%3A49%3A 31&
is_total_fee_adjust=N&
gmt_payment=2008-10-22+20%3A49%3A50&
gmt_c lose=2008-10-22+20%3A49%3A46&
gmt_refund=2008-10-29+19%3A38%3A25 &
use_coupon=N&notify_time=2009-08-12+11%3A08%3A32&
notify_type=trade _status_sync&
notify_id=70fec0c2730b27528665af4517c27b95&
sign_type=DSA &
sign=_p_w_l_h_j0b_gd_aejia7n_ko4_m%252Fu_w_jd3_nx_s_k_mxus9_hoxg _y_r_lunli_pmma29_t_q%253D%253D&
extra_common_param=%E4%BD%A0 %E5%A5%BD%EF%BC%8C%E8%BF%99%E6%98%AF%E6%B5%8B%E8% AF%95%E5%95%86%E6%88%B7%E7%9A%84%E5%B9%BF%E5%91%8A% E3%80%82
 */



		return new ResponseEntity<String>("success",HttpStatus.OK);


	}

	private String getOrginString(Map<String,String> paramMap){

		Set<String> keySet=paramMap.keySet();
		keySet.remove("sign_type");
		keySet.remove("sign");

		List<String> list=new ArrayList<String>(keySet);

		Collections.sort(new ArrayList<String>(keySet));
		StringBuffer sb=new StringBuffer();

		for(String key:list){

			String value=paramMap.get(key);
			try {
				value=URLDecoder.decode(value,"UTF-8");
				sb.append(key).append("=").append(value).append("&");
			} catch (UnsupportedEncodingException e) {
				throw new IllegalArgumentException(e);
			}
		}

		sb.deleteCharAt(sb.length()-1);

		return sb.toString();

	}







	/*
	http://www.xxx.com/alipay/notify_url.php?
	service=alipay.wap.trade.create.direct &
	sign=Rw/y4ROnNicXhaj287Fiw5pvP6viSyg53H3iNiJ61D3YVi7zGniG2680pZv 6rakMCeXX++q9XRLw8Rj6I1//qHrwMAHS1hViNW6hQYsh2TqemuL/xjXRCY3vj m1HCoZOUa5zF2jU09yG23MsMIUx2FAWCL/rgbcQcOjLe5FugTc=&
	sec_id=00 01&
	v=2.0&
	notify_data=g3ivqicRwI9rI5jgmSHSU2osBXV1jcxohapSAPjx4f6qiqso AzstaRWuPuutE0gxQwzMOtwL3npZqWO3Z89J4w4dXIY/fvOLoTNn8FjExAf7O ozoptUS6suBhdMyo/YJyS3lVALfCeT3s27pYWihHgQgna6cTfgi67H2MbX40xtex IpUnjgxBkmOLai8DPOUI58y4UrVwoXQgdcwnXsfn2OthhUFiFPfpINgEphUAq1n C/EPymP6ciHdTCWRI6l1BgWuCzdFy0MxJLliPSnuLyZTou7f+Z5Mw24FgOacaI SB+1/G+c4XIJVKJwshCDw9Emz+NAWsPvq34FEEQXVAeQRDOphJx8bDqLK 75CGZX+6fx88m5ztq4ykuRUcrmoxZLJ+PiABvYFzi5Yx2uBMP/PmknRmj1HUK EhuVWsXR0t6EWpJFXlyQA4uxbShzncWDigndD7wbfNtkNLg5xMSFFIKay+4Yz JK68H9deW4xqk4JYTKsv8eom9Eg9MrJZiIrFkFpVYPuaw0y/n61UEFYdzEQZz +garCmMYehEAQCGibYUQXBlf1iwTOZdqJIxdgCpSX21MIa9N9jicmFu8OXWZ JkdN+UrSyvIcpzRori+U6522ovMz5Z8EzVTfcUENu+d
	 */




	private static final String SIGN_TMP="service=${service}&v=${v}&sec_id=${sec_id}&notify_data=${notify_data}";

	@RequestMappingProp("alipay-mobile-callback")
	public ResponseEntity<String> callbackForMobile(HttpServletRequest request,@RequestParam Map<String,String> allRequestParams,HttpServletResponse response){


		AlipayConfig config=store.getAlipayConfig();

		String secID=allRequestParams.get("sec_id");

		if("0001".equals(secID)){
			allRequestParams.put("notify_data",config.decrypt(allRequestParams.get("notify_data")));
		}


		String fullStr= StringTemplate.generByMap(SIGN_TMP, allRequestParams);

		boolean sign=false;
		if("0001".equals(secID)) {

			sign=config.verifyWithRSA(fullStr, allRequestParams.get("sign"));
		}else if("MD5".equals(secID)) {
			sign=config.verifyWithMD5(fullStr, allRequestParams.get("sign"));
		}


		if(!sign) {
			return new ResponseEntity<String>("fail", HttpStatus.OK);
		}

		try {

			String xml=allRequestParams.get("notify_data");

			NotifyEntity notify = new NotifyEntity(new ByteArrayInputStream(xml.getBytes("UTF-8")));


			Transaction transaction=transactionDao.getOrderByTransactionID(notify.getTransactionID());

			if(
					transaction.getPrice().equals(notify.getPrice())
					){


//				transactionDao.completePay(transaction.getTransactionID(),notify);

			}

			return new ResponseEntity<String>("success",HttpStatus.OK);

		}catch(Exception e){
			return new ResponseEntity<String>("fail", HttpStatus.OK);

		}



	}



}
