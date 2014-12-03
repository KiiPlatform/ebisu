package com.kii.yankon.providers;

import android.content.ContentValues;
import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.graphics.Color;

import java.util.UUID;

/**
 * Created by Evan on 14/11/27.
 */
public class DBHelper extends SQLiteOpenHelper {

    public static final int DB_VERSION = 1;
    public static final String DB_NAME = "yankon.sqlite";

    public DBHelper(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        createDB(db);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {

    }

    protected void createDB(SQLiteDatabase db) {
        db.execSQL("CREATE TABLE IF NOT EXISTS lights ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "UUID TEXT NOT NULL,"
                        + "ThingID TEXT,"
                        + "name TEXT,"
                        + "color_rel TEXT,"
                        + "color INTEGER,"
                        + "model INTEGER,"
                        + "is_mine BOOL,"
                        + "is_on BOOL,"
                        + "connected BOOL,"
                        + "owned_time INTEGER"
                        + ");"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS models ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "model TEXT,"
                        + "name TEXT,"
                        + "pic TEXT,"
                        + "des TEXT"
                        + ");"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS colors ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "UUID TEXT NOT NULL,"
                        + "name TEXT,"
                        + "value INTEGER,"
                        + "created_time INTEGER"
                        + ");"
        );
        db.execSQL("CREATE VIEW IF NOT EXISTS lights_view AS SELECT "
                + "lights.*,models.name AS m_name, colors.name AS c_name, colors.value"
                + " FROM lights LEFT JOIN models ON lights.model=models.model "
                + " LEFT JOIN colors ON lights.color_rel = colors.UUID;");
        db.execSQL("CREATE TABLE IF NOT EXISTS light_groups ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "UUID TEXT NOT NULL,"
                        + "name TEXT,"
                        + "created_time INTEGER"
                        + ");"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS light_group_rel ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "light_id INTEGER,"
                        + "group_id INTEGER,"
                        + "created_time INTEGER"
                        + ");"
        );
        db.execSQL("CREATE VIEW IF NOT EXISTS light_groups_view AS SELECT "
                + "light_groups.*,(select count(_id) FROM light_group_rel where light_group_rel.group_id=light_groups._id) as num"
                + " FROM light_groups;");
        db.execSQL("CREATE VIEW IF NOT EXISTS group_light_view AS SELECT * FROM light_group_rel,lights "
                + " WHERE light_group_rel.light_id=lights._id;");
        db.execSQL("CREATE TABLE IF NOT EXISTS scenes ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "UUID TEXT NOT NULL,"
                        + "name TEXT,"
                        + "created_time INTEGER"
                        + ");"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS scenes_detail ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "scene_id INTEGER,"
                        + "light_id INTEGER,"
                        + "group_id INTEGER,"
                        + "action_id INTEGER,"
                        + "created_time INTEGER"
                        + ");"
        );
        db.execSQL("CREATE TABLE IF NOT EXISTS actions ("
                        + "_id INTEGER PRIMARY KEY, "
                        + "UUID TEXT NOT NULL,"
                        + "name TEXT,"
                        + "content TEXT,"
                        + "created_time INTEGER"
                        + ");"
        );
        db.execSQL("CREATE TRIGGER IF NOT EXISTS light_group_delete"
                + " BEFORE DELETE ON light_groups"
                + " FOR EACH ROW"
                + " BEGIN"
                + " DELETE FROM light_group_rel WHERE light_group_rel.group_id=old._id;"
                + " END;");
        ContentValues values = new ContentValues();
        values.put("UUID", UUID.randomUUID().toString());
        values.put("name", "Red");
        values.put("value", Color.RED);
        values.put("created_time", 1);
        db.insert("colors", null, values);
        values = new ContentValues();
        values.put("UUID", UUID.randomUUID().toString());
        values.put("name", "Green");
        values.put("value", Color.GREEN);
        values.put("created_time", 2);
        db.insert("colors", null, values);
        values = new ContentValues();
        values.put("UUID", UUID.randomUUID().toString());
        values.put("name", "Blue");
        values.put("value", Color.BLUE);
        values.put("created_time", 3);
        db.insert("colors", null, values);
        values = new ContentValues();
        values.put("UUID", UUID.randomUUID().toString());
        values.put("name", "Black");
        values.put("value", Color.BLACK);
        values.put("created_time", 4);
        db.insert("colors", null, values);


        //TODO Below is mock data, need to be removed
        values = new ContentValues();
        values.put("name", "LYZ_17");
        values.put("model", "model1");
        db.insert("models", null, values);
    }

}
