package com.kii.yankon.providers;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

public class YanKonProvider extends ContentProvider {

    public static final String AUTHORITY = "com.kii.yankon";
    public static final UriMatcher uriMatcher;

    public static final Uri URI_COLORS = Uri.parse("content://" + AUTHORITY + "/colors");

    public static final String TABLE_COLORS = "colors";


    private static final int ID_COLORS = 0;

    static {
        uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        uriMatcher.addURI(AUTHORITY, "colors", ID_COLORS);
    }


    private DBHelper mDBHelper;

    public YanKonProvider() {
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        int ret = 0;
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_COLORS:
                ret = database.delete(TABLE_COLORS,
                        selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
        }
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        long cid = 0;
        switch (uriMatcher.match(uri)) {
            case ID_COLORS:
                cid = database.insertWithOnConflict(TABLE_COLORS, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                return Uri.withAppendedPath(uri, String.valueOf(cid));
        }
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public boolean onCreate() {
        mDBHelper = new DBHelper(getContext());
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_COLORS:
                Cursor c = database.query(TABLE_COLORS, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
        }
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
                      String[] selectionArgs) {
        int ret = 0;
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_COLORS:
                ret = database
                        .update(TABLE_COLORS, values, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
        }
        throw new UnsupportedOperationException("Not yet implemented");
    }
}
