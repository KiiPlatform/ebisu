package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.alipay.AlipayNotify;
import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.entity.OrderStatus;
import com.kii.app.youwill.iap.server.entity.Receipt;
import com.kii.app.youwill.iap.server.entity.Transaction;
import com.kii.platform.ufp.user.UserID;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import javax.servlet.http.HttpServletResponse;
import java.util.HashMap;
import java.util.Map;

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
        //if (true) {
            String tradeStatus = allRequestParams.get("trade_status");
            String transactionID = allRequestParams.get("out_trade_no");
            //tradeStatus = "TRADE_SUCCESS";
            //transactionID = "7da07ea0-6a22-11e4-ab1c-90b8d0235395";
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

}
