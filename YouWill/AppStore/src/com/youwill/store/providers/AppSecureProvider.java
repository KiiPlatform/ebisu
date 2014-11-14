package com.youwill.store.providers;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

/**
 * Created by tian on 14-9-16:下午9:12.
 */
public class AppSecureProvider extends ContentProvider {

    private DBHelper mDBHelper = null;

    @Override
    public boolean onCreate() {
        mDBHelper = new DBHelper(getContext());
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        Cursor c = database
                .query(YouWill.Purchased.VIEW_NAME, projection,
                        YouWill.Application.PACKAGE_NAME + "=" + selection, selectionArgs,
                        null, null, sortOrder);
        MatrixCursor cursor = new MatrixCursor(new String[]{"key"});
        if (c != null && c.moveToFirst()) {
            try {
                cursor.addRow(
                        new String[]{c.getString(c.getColumnIndex(YouWill.Purchased.APP_KEY))});
            } catch (Exception e) {

            }
        }
        return cursor;
    }

    @Override
    public String getType(Uri uri) {
        return "vnd.android.cursor.dir/com.kii.store.sandbox.check";
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }
}
