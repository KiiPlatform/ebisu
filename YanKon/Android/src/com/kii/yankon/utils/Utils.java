package com.kii.yankon.utils;

import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.widget.Toast;

import com.kii.cloud.storage.KiiUser;
import com.kii.yankon.model.Light;
import com.kii.yankon.providers.YanKonProvider;

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

    public static String byteArrayToString(byte[] cmd, char seperator) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < cmd.length; i++) {
            sb.append(seperator);
            sb.append(Utils.ByteToHexString(cmd[i]));
        }
        return sb.toString();
    }

    public static String byteArrayToString(byte[] cmd) {
        return byteArrayToString(cmd, ' ');
    }

    public static void byteArrayToString(byte[] cmd, StringBuilder sb) {
        for (int i = 0; i < cmd.length; i++) {
            sb.append(' ');
            sb.append(Utils.ByteToHexString(cmd[i]));
        }
    }

    public static int unsignedByteToInt(byte b) {
        return (int) b & 0xFF;
    }

    public static int readInt16(byte[] data, int offset) {
        return unsignedByteToInt(data[offset + 1]) * 256 + unsignedByteToInt(data[offset]);
    }

    public static void Int16ToByte(int data, byte[] arr, int pos) {
        arr[pos + 1] = (byte) (data / 256);
        arr[pos] = (byte) (data % 256);
    }

    public static int getRGBColor(byte[] data) {
        return unsignedByteToInt(data[0]) * 256 * 256 + unsignedByteToInt(data[1]) * 256 + unsignedByteToInt(data[2]);
    }

    public static String stringFromBytes(byte[] data) {
        int size = 0;
        while (size < data.length) {
            if (data[size] == 0) {
                break;
            }
            size++;
        }
        return new String(data, 0, size);
    }

    public static int char2int(char c) {
        if (c >= '0' && c <= '9') {
            return c - '0';
        } else if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return 0;
    }

    public static String ByteToHexString(byte b) {
        char[] ch = new char[2];
        int a = unsignedByteToInt(b);
        int t = a / 16;
        if (t < 10) {
            ch[0] = (char) ('0' + t);
        } else {
            ch[0] = (char) ('a' + t - 10);
        }

        t = a % 16;
        if (t < 10) {
            ch[1] = (char) ('0' + t);
        } else {
            ch[1] = (char) ('a' + t - 10);
        }
        return new String(ch);
    }

    public static void controlLight(Context context, int light_id) {
        Light light = null;
        Cursor c = context.getContentResolver().query(YanKonProvider.URI_LIGHTS, null, "_id=" + light_id, null, null);
        if (c != null) {
            if (c.moveToFirst()) {
                light = new Light();
                light.brightness = c.getInt(c.getColumnIndex("brightness"));
                light.CT = c.getInt(c.getColumnIndex("CT"));
                light.color = c.getInt(c.getColumnIndex("color"));
                light.ip = c.getString(c.getColumnIndex("IP"));
                light.is_on = c.getInt(c.getColumnIndex("is_on")) != 0;
            }
            c.close();
        }
        if (light == null) {
            Toast.makeText(context, "Cannot load light info", Toast.LENGTH_SHORT).show();
            return;
        }
        if (KiiUser.isLoggedIn()) {

        }
    }

    public static void controlGroup(Context context, int group_id, boolean is_on) {

    }
}
