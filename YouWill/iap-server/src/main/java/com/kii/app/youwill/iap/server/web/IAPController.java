package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.service.FinishTransactionParam;
import com.kii.app.youwill.iap.server.service.IAPService;
import com.kii.app.youwill.iap.server.service.StartTransactionParam;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/**
 * Created by ethan on 14-8-4.
 */

@RestController
@RequestMapping(value="/iap",produces="application/json")
public class IAPController {

	@Autowired
	private IAPService service;
	@Autowired
	private AppContext appContext;


	@RequestMapping("/startOrder/product/{productID}")
	public String createTransaction(@PathVariable("appID") String appID,
									@PathVariable("productID") String productID,
									@RequestBody String context) {


		StartTransactionParam param=new StartTransactionParam(context);

		param.setProductID(productID);

		return service.startOrderTransaction(param).toString();

	}





	@RequestMapping("/finishOrder/order/{orderID}")
	public String finishTransaction(@PathVariable("appID") String appID,
											 @PathVariable("orderID") String orderID,
											 @RequestBody String context) {

		FinishTransactionParam param=new FinishTransactionParam(context);
		param.setTransactionID(orderID);


		String status=service.finishOrder(param);

		JSONObject result = new JSONObject();
		try {
			result.put("status", status);
		} catch (JSONException e) {
			e.printStackTrace();
		}
		return result.toString();


	}



}
