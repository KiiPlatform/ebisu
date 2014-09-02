/*
 * Copyright (C) 2010 The MobileSecurePay Project
 * All right reserved.
 * author: shiqun.shi@alipay.com
 */

package com.kii.payment;

final class AlixId {
    private static final int BASE_ID = 0;
    public static final int RQF_PAY = BASE_ID + 1;
    public static final int RQF_INSTALL_CHECK = RQF_PAY + 1;
}

final class AlixDefine {
    public static final String VERSION = "version";
    public static final String partner = "partner";
    public static final String action = "action";
    public static final String actionUpdate = "update";
    public static final String data = "data";
    public static final String platform = "platform";
}