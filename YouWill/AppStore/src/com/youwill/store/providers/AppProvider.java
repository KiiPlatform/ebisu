package com.youwill.store.providers;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

/**
 * Created by tian on 14-9-24:上午11:26.
 */
public class AppProvider extends ContentProvider {
    public static final String AUTHORITY = "com.youwill.store";
    public static final Uri URI_APPS = Uri.parse("content://" + AUTHORITY + "/apps");

    public static final String TABLE_APPS = "apps";
    public static final String APP_ID = "appid";
    public static final String APP_INFO = "info";
    public static final String APP_PACKAGE = "package";

    public static final UriMatcher uriMatcher;
    private static final int ID_APPS = 0;

    private DBHelper mDBHelper = null;

    static {
        uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        uriMatcher.addURI(AUTHORITY, "apps", ID_APPS);
    }


    @Override
    public boolean onCreate() {
        mDBHelper = new DBHelper(getContext());
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
                        String sortOrder) {
        SQLiteDatabase mDB = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS: {
                Cursor c = mDB.query(TABLE_APPS,projection, selection, selectionArgs, null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
        }
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        SQLiteDatabase mDB = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS:
                mDB.insertWithOnConflict(TABLE_APPS, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
        }
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
