package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.dao.ReceiptDao;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.entity.OrderStatus;
import com.kii.app.youwill.iap.server.entity.Receipt;
import com.kii.app.youwill.iap.server.entity.Transaction;
import com.kii.app.youwill.iap.server.mm.MMUtil;
import com.kii.platform.ufp.user.UserID;
import net.sf.json.JSONObject;
import net.sf.json.xml.XMLSerializer;
import org.apache.log4j.Logger;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.ResponseBody;
import org.springframework.web.bind.annotation.RestController;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;

/**
 * MMIAP计费结果回调通知处理类
 *
 * @author kimi
 * @dateTime 2013-4-28 下午4:10:26
 */

@RestController
public class MMIAPApiController {

    private Logger log = Logger.getLogger(getClass());

    private Logger mmiap_log = Logger.getLogger("mmiap");

    @Autowired
    private TransactionDao transactionDao;

    @Autowired
    private ReceiptDao receiptDao;

    /**
     * MM计费订单结果通知接口
     *
     * @param result
     * @param request
     * @param response
     * @param model
     * @return 开发者服务器 -> M-Market平台 应答结果
     * @throws Exception
     * @author kimi
     * @dateTime 2012-6-18 下午8:21:33
     */
    @RequestMapping(value = "/iap/callback/mmiap")
    @ResponseBody
    protected ResponseEntity<String> mmiap(@RequestBody String result, HttpServletRequest request,
                                           HttpServletResponse response, Map<String, Object> model) throws Exception {
        if (null == result || "".equals(result)) {
            response.setStatus(400);
            return null;
        }

        XMLSerializer xmlSerializer = new XMLSerializer();
        JSONObject jsonObject = (JSONObject) xmlSerializer.read(result);
        String mmTransID = jsonObject.optString("TransactionID", "");
        if (!MMUtil.verifyRequest(jsonObject)) {
            return new ResponseEntity<String>(getResponse(mmTransID, 1000), HttpStatus.OK);
        }

        String exData = jsonObject.optString("ExData", "");
        Transaction transaction = transactionDao.getOrderByTransactionID(exData);
        OrderStatus status = transactionDao.completeMMPay(transaction, jsonObject);
        if (transaction.getPayStatus() == OrderStatus.pending &&
                (status == OrderStatus.completed || status == OrderStatus.success)) {
            receiptDao.createNewReceipt(Receipt.createReceiptForMM(transaction, jsonObject.toString()),
                    new UserID(transaction.getUserID()));
        }
        return new ResponseEntity<String>(getResponse(mmTransID, 0), HttpStatus.OK);
    }

    public String getResponse(String transactionID, int hRet) {
        XMLSerializer respXMLSerializer = new XMLSerializer();

        JSONObject resp = new JSONObject();
        resp.put("TransactionID", transactionID);
        resp.put("MsgType", "SyncAppOrderResp");
        resp.put("Version", "1.0.0");
        resp.put("hRet", hRet);

        respXMLSerializer.setRootName("SyncAppOrderResp");
        respXMLSerializer.setTypeHintsEnabled(false);
        respXMLSerializer.setNamespace(null, "http://www.monternet.com/dsmp/schemas/");
        return respXMLSerializer.write(resp);
    }
}