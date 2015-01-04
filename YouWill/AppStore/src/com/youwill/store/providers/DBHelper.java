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
                        + YouWill.Application.PACKAGE_NAME + " TEXT,"
                        + YouWill.Application.APP_INFO + " TEXT, "
                        + YouWill.Application.VERSION_CODE + " INTEGER,"
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
                        + YouWill.Purchased.APP_KEY + " TEXT,"
                        + YouWill.Purchased.IS_PURCHASED + " tinyint(1) default 1"
                        + " );"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS "
                        + YouWill.Downloads.TABLE_NAME
                        + " ("
                        + "_id INTEGER PRIMARY KEY,"
                        + YouWill.Downloads.APP_ID + " TEXT NOT NULL UNIQUE,"
                        + YouWill.Downloads.PACKAGE_NAME + " TEXT NOT NULL,"
                        + YouWill.Downloads.DOWNLOAD_ID + " INTEGER"
                        + ");"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS "
                        + YouWill.LocalApps.TABLE_NAME
                        + " ("
                        + "_id INTEGER PRIMARY KEY,"
                        + YouWill.LocalApps.PACKAGE_NAME + " TEXT NOT NULL,"
                        + YouWill.LocalApps.VERSION_CODE + " INTEGER"
                        + ");"
        );

        db.execSQL("CREATE VIEW IF NOT EXISTS "
                        + YouWill.Purchased.VIEW_NAME
                        + " AS "
                        + " SELECT * FROM purchased INNER JOIN apps ON purchased.app_id = apps.app_id "
                        + " LEFT JOIN downloads ON purchased.app_id = downloads.app_id;"
        );

        db.execSQL("CREATE VIEW IF NOT EXISTS "
                        + YouWill.Upgrade.VIEW_NAME
                        + " AS "
                        + " SELECT * FROM localapps INNER JOIN "
                        + " apps ON localapps.package_name = apps.package_name AND apps.version_code > localapps.version_code"
                        + " LEFT JOIN downloads ON apps.app_id = downloads.app_id;"
        );

        db.execSQL("CREATE VIEW IF NOT EXISTS "
                        + YouWill.Application.PURCHASED_VIEW_NAME
                        + " AS "
                        + " SELECT * FROM apps LEFT JOIN purchased ON purchased.app_id = apps.app_id "
                        + " LEFT JOIN downloads ON purchased.app_id = downloads.app_id;"
        );

        db.execSQL("CREATE UNIQUE INDEX apps_id ON apps(app_id);");
        db.execSQL("CREATE UNIQUE INDEX apps_package ON apps(package_name);");
        db.execSQL("CREATE UNIQUE INDEX purchased_id ON purchased(app_id);");
        db.execSQL("CREATE UNIQUE INDEX downloads_id ON purchased(app_id);");
        db.execSQL("CREATE UNIQUE INDEX localapps_package ON localapps(package_name);");
    }
}
