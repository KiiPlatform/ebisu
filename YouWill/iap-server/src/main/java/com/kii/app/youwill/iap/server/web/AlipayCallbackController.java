package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.alipay.AlipayNotify;
import com.kii.app.youwill.iap.server.common.StringTemplate;
import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.entity.*;
import com.kii.platform.ufp.user.UserID;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.ByteArrayInputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.*;

/**
 * Created by ethan on 14-8-11.
 */

@RestController
public class AlipayCallbackController {


    @Autowired
    private ConfigInfoStore store;
    @Autowired
    private TransactionDao transactionDao;

    @Autowired
    private ReceiptDao receiptDao;


    @RequestMapping("/iap/callback/alipay/{authorID}/app/{appID}")
    public ResponseEntity<String> callback(
            @PathVariable("authorID") String authorID,
            @PathVariable("appID") String appID,
            @RequestParam Map<String, String> allRequestParams, HttpServletResponse response) {

        if (AlipayNotify.verify(allRequestParams)) {
            String tradeStatus = allRequestParams.get("trade_status");
            String transactionID = allRequestParams.get("out_trade_no");
            if ("TRADE_FINISHED".equals(tradeStatus)
                    || "TRADE_SUCCESS".equals(tradeStatus)) {
                Transaction transaction = transactionDao.getOrderByTransactionID(transactionID);
                OrderStatus status = transactionDao.completeAlipayPay(transaction, allRequestParams);
                if (transaction.getPayStatus() == OrderStatus.pending &&
                        (status == OrderStatus.completed || status == OrderStatus.success)) {
                    Map<String, String> info = new HashMap<String, String>(allRequestParams);
                    info.put("authorID", authorID);
                    info.put("appID", appID);
                    receiptDao.createNewReceipt(new Receipt(transaction, info),
                            new UserID(transaction.getUserID()));
                }
            } else {
                System.out.println("tradeStatus: " + tradeStatus);
            }
            return new ResponseEntity<String>("success", HttpStatus.OK);
        } else {
            return new ResponseEntity<String>("fail", HttpStatus.OK);
        }


    }

    private String getOrginString(Map<String, String> paramMap) {

        Set<String> keySet = paramMap.keySet();
        keySet.remove("sign_type");
        keySet.remove("sign");

        List<String> list = new ArrayList<String>(keySet);

        Collections.sort(new ArrayList<String>(keySet));
        StringBuffer sb = new StringBuffer();

        for (String key : list) {

            String value = paramMap.get(key);
            try {
                value = URLDecoder.decode(value, "UTF-8");
                sb.append(key).append("=").append(value).append("&");
            } catch (UnsupportedEncodingException e) {
                throw new IllegalArgumentException(e);
            }
        }

        sb.deleteCharAt(sb.length() - 1);

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


    private static final String SIGN_TMP = "service=${service}&v=${v}&sec_id=${sec_id}&notify_data=${notify_data}";

    @RequestMapping("/iap/callback/alipay")
    public ResponseEntity<String> callbackForMobile(HttpServletRequest request, @RequestParam Map<String, String> allRequestParams, HttpServletResponse response) {


        AlipayConfig config = store.getAlipayConfig();

        String secID = allRequestParams.get("sec_id");

        if ("0001".equals(secID)) {
            allRequestParams.put("notify_data", config.decrypt(allRequestParams.get("notify_data")));
        }


        String fullStr = StringTemplate.generByMap(SIGN_TMP, allRequestParams);

        boolean sign = false;
        if ("0001".equals(secID)) {

            sign = config.verifyWithRSA(fullStr, allRequestParams.get("sign"));
        } else if ("MD5".equals(secID)) {
            sign = config.verifyWithMD5(fullStr, allRequestParams.get("sign"));
        }


        if (!sign) {
            return new ResponseEntity<String>("fail", HttpStatus.OK);
        }

        try {

            String xml = allRequestParams.get("notify_data");

            NotifyEntity notify = new NotifyEntity(new ByteArrayInputStream(xml.getBytes("UTF-8")));


            Transaction transaction = transactionDao.getOrderByTransactionID(notify.getTransactionID());

            if (
                    transaction.getPrice().equals(notify.getPrice())
                    ) {


//				transactionDao.completePay(transaction.getTransactionID(),notify);

            }

            return new ResponseEntity<String>("success", HttpStatus.OK);

        } catch (Exception e) {
            return new ResponseEntity<String>("fail", HttpStatus.OK);

        }


    }


}
