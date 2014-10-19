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
    public static final UriMatcher uriMatcher;
    private static final int ID_APPS = 0;

    private DBHelper mDBHelper = null;

    static {
        uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        uriMatcher.addURI(YouWill.AUTHORITY, "apps", ID_APPS);
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
                Cursor c = mDB.query(YouWill.Application.TABLE_NAME, projection, selection, selectionArgs, null, null, sortOrder);
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
    public int bulkInsert(Uri uri, ContentValues[] values) {
        int count = 0;
        SQLiteDatabase mDB = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS: {
                for (ContentValues value : values) {
                    mDB.insertWithOnConflict(YouWill.Application.TABLE_NAME, null, value, SQLiteDatabase.CONFLICT_REPLACE);
                    count++;
                }
                getContext().getContentResolver().notifyChange(uri, null);
            }
            break;
        }
        return count;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        SQLiteDatabase mDB = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS:
                mDB.insertWithOnConflict(YouWill.Application.TABLE_NAME, null, values, SQLiteDatabase.CONFLICT_REPLACE);
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
