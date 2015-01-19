
package com.kii.appstore.secure;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

public class AppStoreSecureSDK {
    private static final String LOG_TAG = "AppStoreSecureSDK";
    public static final int MODE_PRODUCTION = 0;
    public static final int MODE_SANDBOX = 1;
    private static int mode = MODE_PRODUCTION;
    private static final String APPSTORE_PACKAGE = "com.youwill.store";
    private static final String APPSTORE_SANDBOX_PACKAGE = "com.youwill.store.sandbox";
    private static final Uri APPSTORE_URI = Uri.parse("content://com.kii.store.check/");
    private static final Uri APPSTORE_SANDBOX_URI = Uri
            .parse("content://com.kii.store.sandbox.check/");

    private static final String APPSTORE_SIGNATURE = "3082049030820378a0030201020209009cfc41fe236e1396300d06092a864886f70d010105050030818c310b3009060355040613025553311330110603550408130a43616c69666f726e6961311630140603550407130d4d6f756e7461696e2056696577310e300c060355040a1305496e74656c310e300c060355040b1305496e74656c310e300c06035504031305496e74656c3120301e06092a864886f70d0109011611616e64726f696440696e74656c2e636f6d301e170d3132303632363134353934365a170d3339313131323134353934365a30818c310b3009060355040613025553311330110603550408130a43616c69666f726e6961311630140603550407130d4d6f756e7461696e2056696577310e300c060355040a1305496e74656c310e300c060355040b1305496e74656c310e300c06035504031305496e74656c3120301e06092a864886f70d0109011611616e64726f696440696e74656c2e636f6d30820120300d06092a864886f70d01010105000382010d00308201080282010100e76b6eaaf25f7585f0629024bfc1379aacfce419d11ab27ead287b652130cccfe11802ddb4470dd4047543ad1ede391e8f4bffb83e6ccf84aa258244c7ed3e06a187e01c8cbc7d225b79cd886aaa5ce6073bcf1d9ccd4fe5db10ea1a46b7a58a1009cf89cf6858895e6dcd640d7b6aeeffe589c9012687cbff95394614c4ae1c149fb8f69e9a3bad840432a62e97563894624e86498425f1da0e0d65dae9c8c436d8f8c0b88486d7be84bab3d1a4e77b596ff441b74b5f37387e3c31fa6e88bfb8bd9f48b875a42a8b29ddcd6d5c00ece1800a4916b99717f1a161530608f332f78230b70bed70c50f8194e8c34e8b161cad5dc8c5079d299e6f71f523ec7359020103a381f43081f1301d0603551d0e0416041451282821b61534a90ab571d3188a8e2d04c261823081c10603551d230481b93081b6801451282821b61534a90ab571d3188a8e2d04c26182a18192a4818f30818c310b3009060355040613025553311330110603550408130a43616c69666f726e6961311630140603550407130d4d6f756e7461696e2056696577310e300c060355040a1305496e74656c310e300c060355040b1305496e74656c310e300c06035504031305496e74656c3120301e06092a864886f70d0109011611616e64726f696440696e74656c2e636f6d8209009cfc41fe236e1396300c0603551d13040530030101ff300d06092a864886f70d01010505000382010100142bd9dc8f518c5ef62e687efadcc907f9fbe3425ed62cc172be27ddba35b0fe5bc7f100281c573ddc23cc0f516db4ba2246f44e674dc0fb038ce365e3958def4b796337b71dd8fbc29ba6bd8c03720014f69d03a13a8a96d7a7eb59bb126a49465a29c07ec77f672ba4c07ad3680851d20d15dd6bcb8810295de3c3264f8763b0b808a9ea00922728335278f28e6d690a902de4b792f52da8eb42166997ca3ab61082be92321a69b5f325af97fcd85a95b6b4e91d9d8026e0aeab9c5012e24dc23691207a235cde75625ee503d163086f4786686a562ededa290cbda4fbc0015ae4511bcaf98d32220b72e2cb2835163fa5a67c321220b78bcad86faa161195";

    private static final String APPSTORE_SANDBOX_SIGNATURE = "3082031d30820205a00302010"
            + "202045aca7e7e300d06092a864886f70d01010b0500303f310b300906035504061302434"
            + "e310b300906035504081302424a3110300e060355040713074265696a696e673111300f0"
            + "60355040a13084b696920496e632e301e170d3134303931373133303030305a170d33393"
            + "03931313133303030305a303f310b300906035504061302434e310b30090603550408130"
            + "2424a3110300e060355040713074265696a696e673111300f060355040a13084b6969204"
            + "96e632e30820122300d06092a864886f70d01010105000382010f003082010a028201010"
            + "086aebe128200f54aeba846417246e26fb1dccc6814de59d60181bb19e0c2335462f4b0f"
            + "450cda2cf3f290f17b3776f578416d6a6e6cf231233a2f042091101903e09672419100bd"
            + "732f81d83fb294835824b11885b18bd0eb82379ce52712719a39d273f7c5496382ae3f05"
            + "c719a07e89833ba21d84d5ee8d088c93ac6c4cf19dbb7286019dcd3f6e52495bceb37694"
            + "dfc2866d9fc2cc9d56398d5d1b6b44ac797e187384fd457880738ddb27407a90d6aa1b99"
            + "98dab0f22ad31c3367437507ef12269f7f3366866b77a4eac3c0f798e287e0410f23c9c0"
            + "155ed0ce6b0a448bd993afc0527f71ec60393a397dffed21f50768b8a6538cc5ecc179df"
            + "8871a9cb90203010001a321301f301d0603551d0e04160414ba6d333fe547b1ade9e83ec"
            + "2363a06d61141a9a5300d06092a864886f70d01010b0500038201010037b5bb828d3ee49"
            + "3404c5b056c8482564c50fcfad3bcface4bc46638a71e9182ba06f6369483ae381a89845"
            + "b07b1d66cae9b2fce4c6709a133c5726196206dd8e4385004a453db6c78c9e1ce9f44831"
            + "a4e0a6ed308013128482914c3a6062494c6d57d786e476c01e79b7a11614634ab22157de"
            + "8c2a64d86823c5dc07ac0b522bba0aecfed0f5d4d4226ab3671df229aa3b9d5b6d2da027"
            + "4ab5b8cf11a0a350e1c20e147bd0409dda999b11e5392f6568d8ad3cc508578a53751f6b"
            + "4a287d607a800e32a8abd0b2ed3d4b9ff241d42008eaf6ea49aed263f05d295416775b00"
            + "0ad05c7f376c75701466430858ed5b51ccd53b1ff1c18499b5b3a2e71a5bed794";

    public static void setMode(int mode) {
        AppStoreSecureSDK.mode = mode;
    }

    public static String getSecureKey(Context context) throws NameNotFoundException {
        String key = null;
        PackageManager pm = context.getPackageManager();
        PackageInfo packageInfo = pm.getPackageInfo(mode == MODE_PRODUCTION ? APPSTORE_PACKAGE
                : APPSTORE_SANDBOX_PACKAGE,
                PackageManager.GET_SIGNATURES);
        String signatureStr = null;
        if (packageInfo.signatures != null && packageInfo.signatures.length > 0) {
            signatureStr = packageInfo.signatures[0].toCharsString();
        }
        // Check signatures here
        if (!signatureStr.equals((mode == MODE_PRODUCTION ? APPSTORE_SIGNATURE
                : APPSTORE_SANDBOX_SIGNATURE))) {
            Log.e(LOG_TAG, "sign:[" + signatureStr + "] is wrong, check you appstore package");
            return null;
        }
        Cursor cursor = null;
        try {
            cursor = context.getContentResolver().query(
                    mode == MODE_PRODUCTION ? APPSTORE_URI : APPSTORE_SANDBOX_URI, null,
                    context.getPackageName(), null, null);
            if (cursor.moveToFirst()) {
                key = cursor.getString(0);
            }
        } catch (Exception e) {
            Log.e(LOG_TAG, Log.getStackTraceString(e));
        } finally {
            if (cursor != null) {
                try {
                    cursor.close();
                } catch (Exception e1) {
                    Log.e(LOG_TAG, Log.getStackTraceString(e1));
                }
            }
        }
        return key;
    }
}
