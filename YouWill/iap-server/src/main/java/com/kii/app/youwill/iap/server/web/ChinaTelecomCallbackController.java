package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Map;

/**
 * Created by ethan on 14-8-11.
 */

@RestController
public class ChinaTelecomCallbackController {

    @Autowired
    private TransactionDao transactionDao;

    @Autowired
    private ReceiptDao receiptDao;

    private static String response_if1 =
            "<sms_pay_check_resp>" +
                    "   <cp_order_id>%s</cp_order_id>" +
                    "   <correlator>%s</correlator>" +
                    "   <game_account>%s</game_account>" +
                    "   <fee>%d</fee>" +
                    "   <if_pay>%d</if_pay>" +
                    "   <order_time>%s</order_time>" +
                    "</sms_pay_check_resp>";

    private static String getResponse_if2 =
            "<cp_notify_resp>" +
            "   <h_ret>0</h_ret>" +
            "   <cp_order_id>%s</cp_order_id>" +
            "</cp_notify_resp>";

    @RequestMapping("/iap/callback/egame/if1")
    public ResponseEntity<String> callback_if1(
            @RequestParam Map<String, String> allRequestParams, HttpServletResponse response) {
        String cp_order_id = allRequestParams.get("cp_order_id");
        String correlator = allRequestParams.get("correlator");
        String order_time = allRequestParams.get("order_time ");
        String method = allRequestParams.get("method");
        String sign = allRequestParams.get("sign");

        String game_account = "richard.liang@kii.com";
        String result = String.format(response_if1, cp_order_id, correlator, game_account, 1, 0, order_time);
        return new ResponseEntity<String>(result, HttpStatus.OK);
    }

    @RequestMapping("/iap/callback/egame/if2")
    public ResponseEntity<String> callback_if2(
            @RequestParam Map<String, String> allRequestParams, HttpServletResponse response) {

        StringBuffer sb = new StringBuffer();
        for (String key : allRequestParams.keySet()) {
            sb.append(key + ":" + allRequestParams.get(key) + ";\r\n");
        }
        log(sb.toString());

        String cp_order_id = allRequestParams.get("cp_order_id");
        String result = String.format(getResponse_if2, cp_order_id);
        return new ResponseEntity<String>("success", HttpStatus.OK);

    }

    private void log(String log) {
        File file = new File("/tmp", "china_telecom.log");
        FileWriter writer = null;
        try {
            writer = new FileWriter(file, true);
            writer.write(log);
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (writer != null) {
                try {
                    writer.close();
                } catch (IOException e) {
                    //
                }
            }
        }
    }

}
