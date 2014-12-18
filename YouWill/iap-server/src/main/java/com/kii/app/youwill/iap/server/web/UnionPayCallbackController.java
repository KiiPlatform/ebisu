package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.alipay.AlipayNotify;
import com.kii.app.youwill.iap.server.common.StringTemplate;
import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.entity.*;
import com.kii.app.youwill.iap.server.unionpay.conf.UpmpConfig;
import com.kii.app.youwill.iap.server.unionpay.service.UpmpService;
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
public class UnionPayCallbackController {


    @Autowired
    private ConfigInfoStore store;
    @Autowired
    private TransactionDao transactionDao;

    @Autowired
    private ReceiptDao receiptDao;


    @RequestMapping("/iap/callback/unionpay")
    public ResponseEntity<String> callback(
            @RequestParam Map<String, String> allRequestParams,
            HttpServletRequest request, HttpServletResponse response) {
        System.out.println("=================getQueryString");
        System.out.println(request.getQueryString());
        logParams(allRequestParams);
        System.out.println("=================");
        boolean verified = UpmpService.verifySignature(allRequestParams);
        if (!verified) {
            logParams(allRequestParams);
            return new ResponseEntity<String>("success", HttpStatus.BAD_REQUEST);
        }

        if (!"00".equals(allRequestParams.get("transStatus"))) {
            logParams(allRequestParams);
            return new ResponseEntity<String>("success", HttpStatus.OK);
        }

        String transactionID = allRequestParams.get("reqReserved");
        Transaction transaction = transactionDao.getOrderByTransactionID(transactionID);
        OrderStatus status = transactionDao.completeUnionPay(transaction, allRequestParams);
        if (transaction.getPayStatus() == OrderStatus.pending &&
                (status == OrderStatus.completed || status == OrderStatus.success)) {
            receiptDao.createNewReceipt(Receipt.createReceiptForUnionPay(transaction, allRequestParams),
                    new UserID(transaction.getUserID()));
        }
        return new ResponseEntity<String>("success", HttpStatus.OK);
    }

    void logParams(Map<String, String> params) {
        /*
        HashMap<String, String> map = new HashMap<String, String>();
        map.put("orderTime", "20141212155323");
        map.put("settleDate", "1212");
        map.put("orderNumber", "41e14fb081bb11e4a4eb90b8d0467bc7");
        map.put("exchangeRate", "0");
        map.put("signature", "19e9fe4eba8a14b7630229e5cf2e90bc");
        map.put("settleCurrency", "156");
        map.put("signMethod", "MD5");
        map.put("transType", "01");
        map.put("respCode", "00");
        map.put("charset", "UTF-8");
        map.put("sysReserved", "{traceTime=1212155323&acqCode=00215800&traceNumber=009739}");
        map.put("version", "1.0.0");
        map.put("settleAmount", "1");
        map.put("transStatus", "00");
        map.put("reqReserved", "41e14fb0-81bb-11e4-a4eb-90b8d0467bc7");
        map.put("merId", "880000000001696");
        map.put("qn", "201412121553230097397");
        */

        System.out.println("============================================");
        for (String key : params.keySet()) {
            System.out.println(key + " : " + params.get(key));
        }
        System.out.println("============================================");
    }

    @RequestMapping("/iap/query/unionpay/{transaction_id}")
    public ResponseEntity<String> callback(
            @PathVariable("transaction_id") String transactionID,
            @RequestParam Map<String, String> allRequestParams,
            HttpServletRequest request, HttpServletResponse response) {
        Map<String, String> req = new HashMap<String, String>();
        req.put("version", UpmpConfig.VERSION);
        req.put("charset", UpmpConfig.CHARSET);
        req.put("transType", "01");
        req.put("merId", UpmpConfig.MER_ID);
        req.put("orderTime", "20141217153553");
        req.put("orderNumber", "54aae09085bf11e4978a90b8d0235395");
        Map<String, String> resp = new HashMap<String, String>();
        boolean validResp = UpmpService.query(req, resp);
        if (validResp) {
            return new ResponseEntity<String>(resp.get("transStatus"), HttpStatus.OK);
        } else {
            return new ResponseEntity<String>("InvalidResponse", HttpStatus.OK);
        }
    }
}
