
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

    private static final String APPSTORE_SIGNATURE = "3082031d30820205a003020102020479ecfc45300d06092a864886f70d01010b0500303f310b300906035504061302434e310b300906035504081302424a3110300e060355040713074265696a696e673111300f060355040a13084b696920496e632e301e170d3134303931373132353635325a170d3339303931313132353635325a303f310b300906035504061302434e310b300906035504081302424a3110300e060355040713074265696a696e673111300f060355040a13084b696920496e632e30820122300d06092a864886f70d01010105000382010f003082010a0282010100b40efa3b7aeb053dcd699fff195480d71d93b5f0266a67d1c587e8f4db221ae5d2fd5737cbcd9e24c47108c68cf6478ae89ce8d74ee2396a50558024a1e5908e7ce80630f74efe56dfc64970eb537ae6720ee470f84d5f60b4bc9e78cf0ff7cee62309fb218a6db126ebcd608c4a7e67ba38c7f715822bd4eebc0857dc6edd6e505e3316a4938c832e5dcbb8bad634e32c369fc44f0ce938c84b4f7e23c1469c5f58ba7c4bd75a099e5be0d072c2632bf4f1e2019a1b94a2d783cdbb8aeb872f1a20a5139aafec8dc813d8f9624ff59f1bfe1d305fd2687dd838b457600b6ffa037c36646b64617a55326ac1077aa7d57cd2f33d5493ecc3247ccf264825cbef0203010001a321301f301d0603551d0e041604144e56770b3b21f1331cdeb3304f8a46dd73be1d6d300d06092a864886f70d01010b050003820101009e616b7e33f185d5cc98d2f07490cf0c01fb9b3552755df028e87bdeff909e27c22f1f8cde6fc921d65506895e6ee2f19b348ca8e570f62bf1d83db68942525a08efc31ebb671153b741829212706b3df6e458fefbd29fb25dc6524da98ebdb0399ac29503ddd85261d3843662ded663a7b70bc6f40954edf249a64c65b514396aad8b3025e29e370b5cbfbc32465c73dc08fea784a99866c19690cd857b606814ebeef5eca16dbb6a4a622e7f0dbd5e0cbbfdb47c7979a9a2e41e831b1ea0a7ccd4847eb6590d0efaa845036cebc7c718650294daec43d19c9aaa93a16b0f19fbfd1f816202e4267bf798b483620e11a79d679a6584a08bfce8ade294ac28c7";

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
