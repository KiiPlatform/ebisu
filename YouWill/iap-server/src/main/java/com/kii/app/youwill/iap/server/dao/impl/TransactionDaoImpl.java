package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.alipay.RSA;
import com.kii.app.youwill.iap.server.alipay.YouWillAlipayConfig;
import com.kii.app.youwill.iap.server.common.IAPUtils;
import com.kii.app.youwill.iap.server.dao.ACLOperate;
import com.kii.app.youwill.iap.server.dao.CommBucketOperate;
import com.kii.app.youwill.iap.server.dao.ConfigInfoStore;
import com.kii.app.youwill.iap.server.dao.TransactionDao;
import com.kii.app.youwill.iap.server.entity.*;
import com.kii.app.youwill.iap.server.mm.MMConfig;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.app.youwill.iap.server.service.StartTransactionParam;
import com.kii.app.youwill.iap.server.service.UUIDGeneral;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.ObjectID;
import com.kii.platform.ufp.user.UserID;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * Created by ethan on 14-7-25.
 */
@Component
public class TransactionDaoImpl implements TransactionDao {

    @Autowired
    private CommBucketOperate commDao;
    @Autowired
    private AppContext appContext;
    @Autowired
    private UUIDGeneral uuidGeneral;
    @Autowired
    private ACLOperate aclOper;
    //	@Autowired
//	private AppConfigureOperate configOper;
    @Autowired
    private ConfigInfoStore store;


    private String BUCKET_ID = "transaction";

    private static String[] ALIPAY_ADDITIONAL_FIELDS = new String[]{"total_fee", "buyer_email", "seller_email",};

    private static DateFormat FMT_DAY = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
    private static DateFormat FMT_MONTH = new SimpleDateFormat("yyyy-MM", Locale.US);
    private static DateFormat FMT_YEAR = new SimpleDateFormat("yyyy", Locale.US);
    private static DateFormat FMT_WEEK = new SimpleDateFormat("ww", Locale.US);


    @Override
    public String createNewOrder(Product product, StartTransactionParam param) {
        switch (param.getPayType()) {
            case alipay:
                return createNewOrderForAliPay(product, param);
            case mm:
                return createNewOrderForMM(product, param);
            default:
                return startNewTransaction(product, param);
        }

    }

    private String createNewOrderForMM(Product product, StartTransactionParam param) {
        String payCode = product.getFieldByName("mm_paycode");
        if (payCode == null) {
            return null;
        }
        String transactionID = startNewTransaction(product, param);
        JSONObject jsonObject = new JSONObject();
        try {
            jsonObject.put("app_id", MMConfig.APP_ID);
            jsonObject.put("app_key", MMConfig.APP_KEY);
            jsonObject.put("transaction_id", transactionID);
            jsonObject.put("pay_code", payCode);
        } catch (JSONException e) {
            e.printStackTrace();
            return null;
        }
        return jsonObject.toString();
    }

    private String createNewOrderForAliPay(Product product, StartTransactionParam param) {
        String transactionID = startNewTransaction(product, param);

        StringBuilder sb = new StringBuilder();
        sb.append("partner=\"");
        sb.append(YouWillAlipayConfig.partner);
        sb.append("\"&out_trade_no=\"");
        sb.append(transactionID);
        sb.append("\"&subject=\"");
        try {
            sb.append(URLEncoder.encode(product.getProductName(), "UTF-8"));
        } catch (Exception e) {
            sb.append(product.getProductName());
            e.printStackTrace();
        }
        sb.append("\"&body=\"");
        try {
            sb.append(URLEncoder.encode(product.getDescription(), "UTF-8"));
        } catch (Exception e) {
            sb.append(product.getDescription());
            e.printStackTrace();
        }

        sb.append("\"&total_fee=\"");
        sb.append(product.getPrice());

        try {
            sb.append("\"&notify_url=\"");
            sb.append(URLEncoder.encode(param.getNotifyURL(), "UTF-8"));
            sb.append("\"&return_url=\"");
            sb.append(URLEncoder.encode("http://m.alipay.com", "UTF-8"));
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
        sb.append("\"&service=\"mobile.securitypay.pay");
        sb.append("\"&_input_charset=\"UTF-8");
        sb.append("\"&payment_type=\"1");
        sb.append("\"&seller_id=\"");
        sb.append(YouWillAlipayConfig.partner);

        sb.append("\"&it_b_pay=\"1m\"");
        String sign = RSA.sign(sb.toString(), YouWillAlipayConfig.private_key, "UTF-8");

        try {
            sign = URLEncoder.encode(sign, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }

        sb.append("&sign=\"" + sign + "\"&sign_type=\"RSA\"");
        return sb.toString();
    }

    private String startNewTransaction(Product product, StartTransactionParam param) {
        Transaction transaction = new Transaction(product,
                new UserID(param.getUserID()),
                param.getAuthorID(), param.getAppID());
        transaction.setPayType(param.getPayType());
        transaction.setPrice(param.getPrice());

        transaction.setPayCode(product.getFieldByName("mm_paycode"));

        if (appContext.isSandBox()) {
            transaction.setSandBox(appContext.isSandBox());
        }

        ObjectID id = commDao.addObject(BUCKET_ID, transaction.getJsonObject());

        String transactionID = id.toString();

        aclOper.removeObjectACLForSpecUser(BUCKET_ID, id, ACLOperate.ObjectRight.Write, ACLOperate.SpecUser.ANY_AUTHENTICATED_USER);

        aclOper.removeObjectACLForSpecUser(BUCKET_ID, id, ACLOperate.ObjectRight.Read, ACLOperate.SpecUser.ANY_AUTHENTICATED_USER);

        return transactionID;
    }


    @Override
    public Transaction getOrderByTransactionID(String transactionID) {
        JSONObject obj = null;

        try {
            obj = commDao.getObjByID(BUCKET_ID, transactionID);
        } catch (Exception e) {
            e.printStackTrace();
        }

        if (obj == null) {
            throw new ServiceException(IAPErrorCode.ORDER_NOT_FOUND);
        }
        return new Transaction(obj);
    }


    @Override
    public OrderStatus completeAlipayPay(Transaction transaction, Map<String, String> callbackParams) {


        TradeStatus tradeStatus = TradeStatus.TRADE_UNKNOWN;
        try {
            tradeStatus = TradeStatus.valueOf(callbackParams.get("trade_status"));
        } catch (Exception e) {
            e.printStackTrace();
        }

        if (transaction.getPayStatus() == OrderStatus.pending ||
                (transaction.getPayStatus() == OrderStatus.success
                        && tradeStatus == TradeStatus.TRADE_FINISHED)) {

            Map<String, Object> map = new HashMap<String, Object>();
            OrderStatus status = null;

            switch (tradeStatus) {
                case TRADE_SUCCESS:
                    status = OrderStatus.success;
                    break;
                case TRADE_FINISHED:
                    status = OrderStatus.completed;
                    break;
                case TRADE_PENDING:
                    status = OrderStatus.pending;
                    break;
                case TRADE_CLOSED:
                    status = OrderStatus.error;
                    break;
                case TRADE_REFUSE:
                case TRADE_REFUSE_DEALING:
                case TRADE_CANCEL:
                    status = OrderStatus.cancel;
                    break;
            }
            if (status == OrderStatus.pending) {
                return status;
            }

            map.put("payStatus", status.name());
            Date paymentDate = IAPUtils.convertAlipayDate(callbackParams.get("gmt_payment"));
            map.put("payCompleteDate", paymentDate.getTime());
            //fields for report
            map.put("pay_day", FMT_DAY.format(paymentDate));
            map.put("pay_month", FMT_MONTH.format(paymentDate));
            map.put("pay_year", FMT_YEAR.format(paymentDate));
            map.put("pay_week", FMT_WEEK.format(paymentDate));


            map.put("alipayTradeNo", callbackParams.get("trade_no"));
            map.put("modifyDate", System.currentTimeMillis());

            //additional fields
            for (String key : ALIPAY_ADDITIONAL_FIELDS) {
                map.put(key, callbackParams.get(key));
            }

            commDao.updateParticObjWithVer(BUCKET_ID, transaction.getId(), transaction.getVersion(), map);

            return status;

        } else {
            throw new ServiceException(IAPErrorCode.PAY_STATUS_INVALID);
        }

    }

    @Override
    public OrderStatus completePaypalPay(Transaction transaction, String paymentID, PaypalQueryResult result) {
        if (transaction.getPayStatus() != OrderStatus.pending) {
            throw new ServiceException(IAPErrorCode.PAY_STATUS_INVALID);
        }

        Map<String, Object> map = new HashMap<String, Object>();
        OrderStatus status = null;

        switch (result.getState()) {
            case approved:
                status = OrderStatus.completed;
                break;
            case failed:
            case expired:
                status = OrderStatus.error;
                break;
            case canceled:
                status = OrderStatus.cancel;
                break;
            case created:
                status = OrderStatus.pending;
        }
        if (status == OrderStatus.pending) {
            return status;
        }

        map.put("payStatus", status.name());

        map.put("payCompleteDate", result.getUpdateTime());

        map.put("paypalPaymentID", result.getId());
        map.put("modifyDate", new Date());

        commDao.updateParticObjWithVer(BUCKET_ID, transaction.getId(), transaction.getVersion(), map);

        return status;

    }

    public static String[] MM_FIELDS = {
            "TransactionID",
            "CheckID",
            "ActionID",
            "MSISDN",
            "FeeMSISDN",
            "AppID",
            "TradeID",
            "TotalPrice",
            "SubsNumb",
            "SubsSeq",
            "ChannelID",
            "OrderType",
            "OrderID",
    };

    @Override
    public OrderStatus completeMMPay(Transaction transaction, net.sf.json.JSONObject jsonObject) {
        if (transaction.getPayStatus() != OrderStatus.pending) {
            throw new ServiceException(IAPErrorCode.PAY_STATUS_INVALID);
        }
        Map<String, Object> map = new HashMap<String, Object>();
        map.put("payStatus", OrderStatus.completed);
        Date paymentDate = IAPUtils.convertMMPayDate(jsonObject.optString("ActionTime", ""));
        map.put("payCompleteDate", paymentDate.getTime());
        //fields for report
        map.put("pay_day", FMT_DAY.format(paymentDate));
        map.put("pay_month", FMT_MONTH.format(paymentDate));
        map.put("pay_year", FMT_YEAR.format(paymentDate));
        map.put("pay_week", FMT_WEEK.format(paymentDate));
        map.put("modifyDate", System.currentTimeMillis());

        for (String key : MM_FIELDS) {
            map.put("mm" + key, jsonObject.optString(key, ""));
        }
        map.put("mmTotalPrice", String.format("%.2f", jsonObject.optInt("TotalPrice", 0) / 100.0));

        commDao.updateParticObjWithVer(BUCKET_ID, transaction.getId(), transaction.getVersion(), map);
        return OrderStatus.completed;
    }

}
