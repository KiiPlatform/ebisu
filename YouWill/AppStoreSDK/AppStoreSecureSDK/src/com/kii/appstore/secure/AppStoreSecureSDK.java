
package com.kii.appstore.secure;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.database.Cursor;
import android.net.Uri;

public class AppStoreSecureSDK {

    public static final int MODE_PRODUCTION = 0;
    public static final int MODE_SANDBOX = 1;
    private static int mode = MODE_PRODUCTION;
    private static final String APPSTORE_PACKAGE = "com.youwill.store";
    private static final String APPSTORE_SANDBOX_PACKAGE = "com.youwill.store.sandbox";
    private static final Uri APPSTORE_URI = Uri.parse("content://com.kii.store.check/");
    private static final Uri APPSTORE_SANDBOX_URI = Uri
            .parse("content://com.kii.store.sandbox.check/");

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

        Cursor cursor = null;
        try {
            cursor = context.getContentResolver().query(
                    mode == MODE_PRODUCTION ? APPSTORE_URI : APPSTORE_SANDBOX_URI, null,
                    context.getPackageName(), null, null);
            key = cursor.getString(0);
        } catch (Exception e) {
        } finally {
            if (cursor != null) {
                try {
                    cursor.close();
                } catch (Exception e1) {
                }
            }
        }
        return key;
    }
}
