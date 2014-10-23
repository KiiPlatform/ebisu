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

    private static final int ID_PURCHASED = 1;

    private static final int ID_DOWNLOADS = 2;

    private static final int ID_LOCALAPPS = 3;

    private static final int ID_UPGRADE = 4;

    private DBHelper mDBHelper = null;

    static {
        uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        uriMatcher.addURI(YouWill.AUTHORITY, "apps", ID_APPS);
        uriMatcher.addURI(YouWill.AUTHORITY, "purchased", ID_PURCHASED);
        uriMatcher.addURI(YouWill.AUTHORITY, "downloads", ID_DOWNLOADS);
        uriMatcher.addURI(YouWill.AUTHORITY, "local_apps", ID_LOCALAPPS);
        uriMatcher.addURI(YouWill.AUTHORITY, "upgrade", ID_UPGRADE);
    }


    @Override
    public boolean onCreate() {
        mDBHelper = new DBHelper(getContext());
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
                        String sortOrder) {
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS: {
                Cursor c = database
                        .query(YouWill.Application.TABLE_NAME, projection, selection, selectionArgs,
                                null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_PURCHASED: {
                Cursor c = database
                        .query(YouWill.Purchased.VIEW_NAME, projection, selection, selectionArgs,
                                null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_DOWNLOADS: {
                Cursor c = database
                        .query(YouWill.Downloads.TABLE_NAME, projection, selection, selectionArgs,
                                null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_LOCALAPPS: {
                Cursor c = database
                        .query(YouWill.LocalApps.TABLE_NAME, projection, selection, selectionArgs,
                                null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_UPGRADE: {
                Cursor c = database
                        .query(YouWill.Upgrade.VIEW_NAME, projection, selection, selectionArgs,
                                null, null, sortOrder);
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
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS: {
                for (ContentValues value : values) {
                    database.insertWithOnConflict(YouWill.Application.TABLE_NAME, null, value,
                            SQLiteDatabase.CONFLICT_REPLACE);
                    count++;
                }
                getContext().getContentResolver().notifyChange(uri, null);
            }
            break;
            case ID_PURCHASED: {
                for (ContentValues value : values) {
                    database.insertWithOnConflict(YouWill.Purchased.TABLE_NAME, null, value, SQLiteDatabase.CONFLICT_REPLACE);
                    count++;
                }
                getContext().getContentResolver().notifyChange(uri, null);
            }
            break;
            case ID_LOCALAPPS: {
                for (ContentValues value : values) {
                    database.insertWithOnConflict(YouWill.LocalApps.TABLE_NAME, null, value, SQLiteDatabase.CONFLICT_REPLACE);
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
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS:
                database.insertWithOnConflict(YouWill.Application.TABLE_NAME, null, values,
                        SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
            case ID_DOWNLOADS:
                database.insertWithOnConflict(YouWill.Downloads.TABLE_NAME, null, values,
                        SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
            case ID_PURCHASED:
                database.insertWithOnConflict(YouWill.Purchased.TABLE_NAME, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
            case ID_LOCALAPPS:
                database.insertWithOnConflict(YouWill.LocalApps.TABLE_NAME, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
        }
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        int ret = 0;
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_APPS:
                ret = database.delete(YouWill.Application.TABLE_NAME, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
            case ID_PURCHASED:
                ret = database.delete(YouWill.Purchased.TABLE_NAME, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
            case ID_DOWNLOADS:
                ret = database.delete(YouWill.Downloads.TABLE_NAME, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
            case ID_LOCALAPPS:
                ret = database.delete(YouWill.LocalApps.TABLE_NAME, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
        }
        return ret;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        int ret = 0;
        SQLiteDatabase database = mDBHelper.getWritableDatabase();
        switch (uriMatcher.match(uri)) {
            case ID_DOWNLOADS:
                ret = database
                        .update(YouWill.Downloads.TABLE_NAME, values, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                break;
        }
        return ret;
    }
}
