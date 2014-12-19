package com.kii.yankon.utils;

import android.graphics.Color;

/**
 * Created by tian on 14/11/30:下午5:46.
 */
public class Constants {

    public static final String INTENT_LOGGED_IN = "com.kii.yankon.intent.log_in";

    public static final String INTENT_LOGGED_OUT = "com.kii.yankon.intent.log_out";

    public static final int DEFAULT_PORT = 5015;

    public static final int DEFAULT_COLOR = Color.BLUE;

    public static final int DEFAULT_CT = 70;

    public static final int DEFAULT_BRIGHTNESS = 80;

    public static final int MIN_CT = 15;

    public static final int MIN_BRIGHTNESS = 15;

    public static final String ACTION_LIGHT_UPDATED = "com.kii.yankon.light.updated";


    public static final byte[] SEARCH_LIGHTS_CMD = new byte[]{00, 00, 00, 00, 0x1e, 00, 01, 01, 00, 00, 00, 0x0a, 00, 00, 00, 00, 0x0a, 01, 00, 00, 00, 0x0a, 0x02, 00, 00, 00, 0x0a, 03, 00, 00, 00, 00, 03, 00, 00, 00};

}
