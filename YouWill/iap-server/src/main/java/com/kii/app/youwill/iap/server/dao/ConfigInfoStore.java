package com.kii.app.youwill.iap.server.dao;

import com.kii.app.youwill.iap.server.entity.AlipayConfig;
import com.kii.app.youwill.iap.server.entity.PaypalConfig;

/**
 * Created by ethan on 14-8-7.
 */
public interface ConfigInfoStore {


	PaypalConfig getPaypalConfig();

	public AlipayConfig getAlipayConfig();


	public String getPaypalDomain();

	public String getIAPSecurityKey();


}
