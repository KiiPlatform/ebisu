package com.youwill.store.providers;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

/**
 * Created by tian on 14-9-24:上午11:25.
 */
public class DBHelper extends SQLiteOpenHelper {

    private static final int DB_VERSION = 1;

    private static final String DB_NAME = "youwill.db";

    public DBHelper(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        createDatabase(db);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {

    }

    protected void createDatabase(SQLiteDatabase db) {
        db.execSQL("CREATE TABLE IF NOT EXISTS "
                        + YouWill.Application.TABLE_NAME
                        + " ("
                        + "_id INTEGER PRIMARY KEY, "
                        + YouWill.Application.APP_ID + " TEXT NOT NULL UNIQUE,"
                        + YouWill.Application.APP_PACKAGE + " TEXT,"
                        + YouWill.Application.APP_INFO + " TEXT, "
                        + YouWill.Application.RECOMMEND_TYPE + " INTEGER, "
                        + YouWill.Application.RECOMMEND_WEIGHT + " INTEGER, "
                        + YouWill.Application.SEARCH_FIELD + " TEXT"
                        + " );"
        );
    }
}
