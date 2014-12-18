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
