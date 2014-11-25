package com.kii.yankon.utils;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.util.TypedValue;

/**
 * Created by tian on 14/11/25:上午9:00.
 */
public class Utils {
    public static float dp2Px(Context context, float dp) {
        Resources resources = context.getResources();
        DisplayMetrics metrics = resources.getDisplayMetrics();
        float px = dp * (metrics.densityDpi / 160f);
        return px;
    }

    public static int px2Dp(Context context, int pixel) {
        int dp = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_PX, pixel,
                context.getResources().getDisplayMetrics());
        return dp;
    }


}
