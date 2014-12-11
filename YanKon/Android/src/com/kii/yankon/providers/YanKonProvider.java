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
    public static final Uri URI_ACTIONS = Uri.parse("content://" + AUTHORITY + "/actions");
    public static final Uri URI_LIGHTS = Uri.parse("content://" + AUTHORITY + "/lights");
    public static final Uri URI_MODELS = Uri.parse("content://" + AUTHORITY + "/models");
    public static final Uri URI_LIGHT_GROUPS = Uri.parse("content://" + AUTHORITY + "/light_groups");
    public static final Uri URI_LIGHT_GROUP_REL = Uri.parse("content://" + AUTHORITY + "/light_group_rel");
    public static final Uri URI_SCENES = Uri.parse("content://" + AUTHORITY + "/scenes");
    public static final Uri URI_SCENES_DETAIL = Uri.parse("content://" + AUTHORITY + "/scenes_detail");

    public static final String TABLE_COLORS = "colors";
    public static final String TABLE_ACTIONS = "actions";
    public static final String TABLE_LIGHTS = "lights";
    public static final String VIEW_LIGHTS = "lights_view";
    public static final String TABLE_MODELS = "models";
    public static final String VIEW_LIGHT_GROUPS = "light_groups_view";
    public static final String TABLE_LIGHT_GROUPS = "light_groups";
    public static final String VIEW_LIGHT_GROUP_REL = "group_light_view";
    public static final String TABLE_LIGHT_GROUP_REL = "light_group_rel";
    public static final String TABLE_SCENES = "scenes";
    public static final String TABLE_SCENES_DETAIL = "scenes_detail";


    private static final int ID_COLORS = 0;
    private static final int ID_ACTIONS = 1;
    private static final int ID_LIGHTS = 2;
    private static final int ID_MODELS = 3;
    private static final int ID_LIGHT_GROUPS = 4;
    private static final int ID_LIGHT_GROUP_REL = 5;
    private static final int ID_SCENES = 6;
    private static final int ID_SCENES_DETAIL = 7;

    static {
        uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        uriMatcher.addURI(AUTHORITY, "colors", ID_COLORS);
        uriMatcher.addURI(AUTHORITY, "actions", ID_ACTIONS);
        uriMatcher.addURI(AUTHORITY, "lights", ID_LIGHTS);
        uriMatcher.addURI(AUTHORITY, "models", ID_MODELS);
        uriMatcher.addURI(AUTHORITY, "light_groups", ID_LIGHT_GROUPS);
        uriMatcher.addURI(AUTHORITY, "light_group_rel", ID_LIGHT_GROUP_REL);
        uriMatcher.addURI(AUTHORITY, "scenes", ID_SCENES);
        uriMatcher.addURI(AUTHORITY, "scenes_detail", ID_SCENES_DETAIL);
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
            case ID_ACTIONS:
                ret = database.delete(TABLE_ACTIONS,
                        selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_LIGHTS:
                ret = database.delete(TABLE_LIGHTS,
                        selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_LIGHT_GROUPS:
                ret = database.delete(TABLE_LIGHT_GROUPS,
                        selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_LIGHT_GROUP_REL:
                ret = database.delete(TABLE_LIGHT_GROUP_REL,
                        selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_SCENES:
                ret = database.delete(TABLE_SCENES,
                        selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_SCENES_DETAIL:
                ret = database.delete(TABLE_SCENES_DETAIL,
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
            case ID_ACTIONS:
                cid = database.insertWithOnConflict(TABLE_ACTIONS, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                return Uri.withAppendedPath(uri, String.valueOf(cid));
            case ID_LIGHTS:
                cid = database.insertWithOnConflict(TABLE_LIGHTS, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                return Uri.withAppendedPath(uri, String.valueOf(cid));
            case ID_LIGHT_GROUPS:
                cid = database.insertWithOnConflict(TABLE_LIGHT_GROUPS, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                return Uri.withAppendedPath(uri, String.valueOf(cid));
            case ID_LIGHT_GROUP_REL:
                cid = database.insertWithOnConflict(TABLE_LIGHT_GROUP_REL, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                return Uri.withAppendedPath(uri, String.valueOf(cid));
            case ID_SCENES:
                cid = database.insertWithOnConflict(TABLE_SCENES, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                getContext().getContentResolver().notifyChange(uri, null);
                return Uri.withAppendedPath(uri, String.valueOf(cid));
            case ID_SCENES_DETAIL:
                cid = database.insertWithOnConflict(TABLE_SCENES_DETAIL, null, values, SQLiteDatabase.CONFLICT_REPLACE);
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
            case ID_COLORS: {
                Cursor c = database.query(TABLE_COLORS, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_ACTIONS: {
                Cursor c = database.query(TABLE_ACTIONS, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_LIGHTS: {
                Cursor c = database.query(VIEW_LIGHTS, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_MODELS: {
                Cursor c = database.query(TABLE_MODELS, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_LIGHT_GROUPS: {
                Cursor c = database.query(VIEW_LIGHT_GROUPS, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_LIGHT_GROUP_REL: {
                Cursor c = database.query(VIEW_LIGHT_GROUP_REL, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_SCENES: {
                Cursor c = database.query(TABLE_SCENES, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
            case ID_SCENES_DETAIL: {
                Cursor c = database.query(TABLE_SCENES_DETAIL, projection, selection, selectionArgs,
                        null, null, sortOrder);
                c.setNotificationUri(getContext().getContentResolver(), uri);
                return c;
            }
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
            case ID_ACTIONS:
                ret = database
                        .update(TABLE_ACTIONS, values, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_LIGHTS:
                ret = database
                        .update(TABLE_LIGHTS, values, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_LIGHT_GROUPS:
                ret = database
                        .update(TABLE_LIGHT_GROUPS, values, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_SCENES:
                ret = database
                        .update(TABLE_SCENES, values, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
            case ID_SCENES_DETAIL:
                ret = database
                        .update(TABLE_SCENES_DETAIL, values, selection, selectionArgs);
                getContext().getContentResolver().notifyChange(uri, null);
                return ret;
        }
        throw new UnsupportedOperationException("Not yet implemented");
    }
}
