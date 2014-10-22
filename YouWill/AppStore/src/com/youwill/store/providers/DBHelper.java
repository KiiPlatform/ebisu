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
                        + YouWill.Application.AGE_CATEGORY + " INTEGER, "
                        + YouWill.Application.RECOMMEND_TYPE + " INTEGER, "
                        + YouWill.Application.RECOMMEND_WEIGHT + " INTEGER, "
                        + YouWill.Application.SEARCH_FIELD + " TEXT"
                        + " );"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS "
                        + YouWill.Purchased.TABLE_NAME
                        + " ("
                        + "_id INTEGER PRIMARY KEY, "
                        + YouWill.Purchased.APP_ID + " TEXT NOT NULL UNIQUE,"
                        + YouWill.Purchased.APP_KEY + " TEXT"
                        + " );"
        );
        db.execSQL("CREATE VIEW IF NOT EXISTS "
                        + YouWill.Purchased.VIEW_NAME
                        + " AS "
                        + " SELECT * FROM "
                        + YouWill.Application.TABLE_NAME
                        + ","
                        + YouWill.Purchased.TABLE_NAME
                        + " WHERE "
                        + YouWill.Application.TABLE_NAME
                        + ".app_id = "
                        + YouWill.Purchased.TABLE_NAME
                        + ".app_id;"
        );
        /**
         * Mockup data
         */
        db.execSQL("insert into purchased values (1, 'f363bc5c', 'test');");
        db.execSQL("insert into purchased values (2, 'a7e0a105', 'test');");
        db.execSQL("insert into purchased values (3, '1fbe85a3', 'test');");
        db.execSQL("insert into purchased values (4, '255ab961', 'test');");
        db.execSQL("insert into purchased values (5, '58ec3c91', 'test');");
        db.execSQL("insert into purchased values (6, '471f6a0a', 'test');");
        db.execSQL("insert into purchased values (7, 'a6b9f9ed', 'test');");
    }
}
