package com.kii.yankon.utils;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.text.TextUtils;

import com.kii.yankon.App;
import com.kii.yankon.providers.YanKonProvider;

/**
 * Created by Evan on 15/1/22.
 */
public class DataHelper {

    public static int getLightIdByMac(String mac) {
        if (TextUtils.isEmpty(mac))
            return -1;
        Cursor cursor = App.getApp().getContentResolver()
                .query(YanKonProvider.URI_LIGHTS, new String[]{"_id"}, "MAC=?",
                        new String[]{mac}, null);
        try {
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.getInt(0);
            }
            return -1;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    public static String getLightMacById(int lightId) {
        Cursor cursor = App.getApp().getContentResolver()
                .query(YanKonProvider.URI_LIGHTS, new String[]{"mac"}, "_id=?",
                        new String[]{Integer.toString(lightId)}, null);
        try {
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.getString(0);
            }
            return "";
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    public static String getGroupById(int groupId) {
        Cursor cursor = App.getApp().getContentResolver()
                .query(YanKonProvider.URI_LIGHT_GROUPS, new String[]{"objectID"}, "_id=?",
                        new String[]{Integer.toString(groupId)}, null);
        try {
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.getString(0);
            }
            return "";
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    public static int getGroupIdByObjId(String objId) {
        if (TextUtils.isEmpty(objId))
            return -1;
        Cursor cursor = App.getApp().getContentResolver()
                .query(YanKonProvider.URI_LIGHT_GROUPS, new String[]{"_id"}, "objectID=?",
                        new String[]{objId}, null);
        try {
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.getInt(0);
            }
            return -1;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    public static String getSceneById(int sceneId) {
        Cursor cursor = App.getApp().getContentResolver()
                .query(YanKonProvider.URI_SCENES, new String[]{"objectID"}, "_id=?",
                        new String[]{Integer.toString(sceneId)}, null);
        try {
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.getString(0);
            }
            return "";
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    public static void deleteLightById(int id) {
        if (id < 0) return;
        ContentValues cv = new ContentValues();
        cv.put("deleted", 1);
        cv.put("synced", false);
        ContentResolver cr = App.getApp().getContentResolver();
        cr.update(YanKonProvider.URI_LIGHTS, cv, "_id=?",
                new String[]{Integer.toString(id)});

        String mac = getLightMacById(id);
        if (!TextUtils.isEmpty(mac)) {
            cr.update(YanKonProvider.URI_SCHEDULE, cv, "light_id=(?)",
                    new String[]{mac});
        }

        cv = new ContentValues();
        cv.put("synced", false);
        Cursor c = cr.query(YanKonProvider.URI_LIGHT_GROUP_REL, new String[]{"group_id"}, "light_id=" + id, null, null);
        if (c != null) {
            while (c.moveToNext()) {
                int group_id = c.getInt(0);
                cr.update(YanKonProvider.URI_LIGHT_GROUPS, cv, "_id=" + group_id, null);
            }
            c.close();
        }
        cr.delete(YanKonProvider.URI_LIGHT_GROUP_REL, "light_id=" + id, null);
        c = cr.query(YanKonProvider.URI_SCENES_DETAIL, new String[]{"scene_id"}, "light_id=" + id, null, null);
        if (c != null) {
            while (c.moveToNext()) {
                int scene_id = c.getInt(0);
                cr.update(YanKonProvider.URI_SCENES, cv, "_id=" + scene_id, null);
            }
            c.close();
        }
        cr.delete(YanKonProvider.URI_SCENES_DETAIL, "light_id=" + id, null);
    }

    public static void deleteLightGroupById(int id) {
        if (id < 0) return;
        ContentValues cv = new ContentValues();
        cv.put("deleted", 1);
        cv.put("synced", false);
        ContentResolver cr = App.getApp().getContentResolver();
        cr.update(YanKonProvider.URI_LIGHT_GROUPS, cv, "_id=?",
                new String[]{Integer.toString(id)});
        cr.delete(YanKonProvider.URI_LIGHT_GROUP_REL, "group_id=" + id, null);

        String objectID = getGroupById(id);
        if (!TextUtils.isEmpty(objectID)) {
            cr.update(YanKonProvider.URI_SCHEDULE, cv, "group_id=(?)",
                    new String[]{objectID});
        }

        cv = new ContentValues();
        cv.put("synced", false);
        Cursor c = cr.query(YanKonProvider.URI_SCENES_DETAIL, new String[]{"scene_id"}, "group_id=" + id, null, null);
        if (c != null) {
            while (c.moveToNext()) {
                int scene_id = c.getInt(0);
                cr.update(YanKonProvider.URI_SCENES, cv, "_id=" + scene_id, null);
            }
            c.close();
        }
        cr.delete(YanKonProvider.URI_SCENES_DETAIL, "group_id=" + id, null);
    }

    public static void deleteSceneById(int id) {
        if (id < 0) return;
        ContentValues cv = new ContentValues();
        cv.put("deleted", 1);
        cv.put("synced", false);
        ContentResolver cr = App.getApp().getContentResolver();
        cr.update(YanKonProvider.URI_SCENES, cv, "_id=?",
                new String[]{Integer.toString(id)});
        cr.delete(YanKonProvider.URI_SCENES_DETAIL, "scene_id=" + id, null);

        String objectID = getSceneById(id);
        if (!TextUtils.isEmpty(objectID)) {
            cr.update(YanKonProvider.URI_SCHEDULE, cv, "scene_id=(?)",
                    new String[]{objectID});
        }
    }

    public static void deleteScheduleById(int id) {
        if (id < 0) return;
        ContentValues cv = new ContentValues();
        cv.put("deleted", 1);
        cv.put("synced", false);
        ContentResolver cr = App.getApp().getContentResolver();
        cr.update(YanKonProvider.URI_SCHEDULE, cv, "_id=?",
                new String[]{Integer.toString(id)});
    }
}
