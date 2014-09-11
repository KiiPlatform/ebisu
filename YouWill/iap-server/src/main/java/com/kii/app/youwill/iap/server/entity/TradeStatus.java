package com.kii.app.youwill.iap.server.entity;

/**
* Created by ethan on 14-8-13.
*/
public enum TradeStatus {

	WAIT_BUYER_PAY,
	TRADE_CLOSED,
	TRADE_SUCCESS,
	TRADE_PENDING,
	TRADE_FINISHED,

	TRADE_REFUSE,
	TRADE_REFUSE_DEALING,
	TRADE_CANCEL,
    TRADE_UNKNOWN,
}
