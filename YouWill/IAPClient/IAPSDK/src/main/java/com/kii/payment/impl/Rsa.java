package com.kii.payment.impl;

import android.util.Log;
import com.kii.payment.Utils;


import java.security.KeyFactory;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;

/**
 * Created by tian on 14-1-3.
 */
public class Rsa {

    private static final String ALGORITHM = "RSA";

    public static final String SIGN_ALGORITHMS = "SHA1WithRSA";
    private static final String CHARSET = "UTF-8";

    public static String sign(String content, String privateKey) {
        try {
            Utils.log(ALGORITHM, "sign, content is " + content);
            Utils.log(ALGORITHM, "sign, privateKey is " + privateKey);
            PKCS8EncodedKeySpec spec = new PKCS8EncodedKeySpec(
                    Base64.decode(privateKey));
            KeyFactory factory = KeyFactory.getInstance(ALGORITHM, "BC");
            PrivateKey priKey = factory.generatePrivate(spec);

            java.security.Signature signature = java.security.Signature
                    .getInstance(SIGN_ALGORITHMS);

            signature.initSign(priKey);
            signature.update(content.getBytes(CHARSET));

            byte[] signed = signature.sign();

            return Base64.encode(signed);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return null;
    }

    public static boolean doCheck(String content, String sign, String publicKey) {
        try {
            KeyFactory keyFactory = KeyFactory.getInstance(ALGORITHM, "BC");
            byte[] encodedKey = Base64.decode(publicKey);
            PublicKey pubKey = keyFactory
                    .generatePublic(new X509EncodedKeySpec(encodedKey));

            java.security.Signature signature = java.security.Signature
                    .getInstance(SIGN_ALGORITHMS);

            signature.initVerify(pubKey);
            Log.i("Result", "initVerify, key is " + pubKey);
            signature.update(content.getBytes(CHARSET));
            Log.i("Result", "content :   " + content);
            Log.i("Result", "sign:   " + sign);
            boolean isVerified = signature.verify(Base64.decode(sign));
            Log.i("Result", "isVerified = " + isVerified);
            return isVerified;
        } catch (Exception e) {
            e.printStackTrace();
        }

        return false;
    }
}
