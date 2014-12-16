package com.kii.app.youwill.iap.server.unionpay.util;

import com.kii.app.youwill.iap.server.unionpay.conf.UpmpConfig;

import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Created by liangyx on 12/10/14.
 */
public class UpmpMd5Encrypt {
    public static String md5(String str) {

        if (str == null) {
            return null;
        }

        MessageDigest messageDigest = null;

        try {
            messageDigest = MessageDigest.getInstance(UpmpConfig.SIGN_TYPE);
            messageDigest.reset();
            messageDigest.update(str.getBytes(UpmpConfig.CHARSET));
        } catch (NoSuchAlgorithmException e) {

            return str;
        } catch (UnsupportedEncodingException e) {
            return str;
        }

        byte[] byteArray = messageDigest.digest();

        StringBuffer md5StrBuff = new StringBuffer();

        for (int i = 0; i < byteArray.length; i++) {
            if (Integer.toHexString(0xFF & byteArray[i]).length() == 1)
                md5StrBuff.append("0").append(Integer.toHexString(0xFF & byteArray[i]));
            else
                md5StrBuff.append(Integer.toHexString(0xFF & byteArray[i]));
        }

        return md5StrBuff.toString();
    }

}
