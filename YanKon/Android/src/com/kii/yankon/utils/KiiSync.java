package com.kii.yankon.utils;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiBucket;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.KiiServerCodeEntry;
import com.kii.cloud.storage.KiiServerCodeEntryArgument;
import com.kii.cloud.storage.KiiServerCodeExecResult;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.query.KiiQueryResult;
import com.kii.yankon.App;
import com.kii.yankon.providers.YanKonProvider;

import org.json.JSONArray;
import org.json.JSONObject;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.util.Log;

import java.util.List;

/**
 * Created by Evan on 14/12/20.
 */
public class KiiSync {

    private static final String LOG_TAG = "KiiSync";

    private static boolean isSyncing = false;

    public static boolean syncLights(Context context, Cursor cursor) {
        KiiUser kiiUser = KiiUser.getCurrentUser();
        if (kiiUser == null) {
            return false;
        }
        boolean syncResult = true;
        KiiBucket bucket = kiiUser.bucket("lights");
        if (cursor != null) {
            do {
                boolean synced = cursor.getInt(cursor.getColumnIndex("synced")) > 0;
                if (synced) {
                    continue;
                }
                int light_id = cursor.getInt(cursor.getColumnIndex("_id"));
                KiiObject lightObj;
                String mac = cursor.getString(cursor.getColumnIndex("MAC"));
                lightObj = bucket.object(mac);
                lightObj.set("name", cursor.getString(cursor.getColumnIndex("name")));
                lightObj.set("model", cursor.getString(cursor.getColumnIndex("model")));
                lightObj.set("remote_pwd", cursor.getString(cursor.getColumnIndex("remote_pwd")));
                lightObj.set("admin_pwd", cursor.getString(cursor.getColumnIndex("admin_pwd")));
                lightObj.set("MAC", mac);
                lightObj.set("light_id", light_id);
//                lightObj.set("brightness", cursor.getInt(cursor.getColumnIndex("brightness")));
//                lightObj.set("CT", cursor.getInt(cursor.getColumnIndex("CT")));
//                lightObj.set("color", cursor.getInt(cursor.getColumnIndex("color")));
//                lightObj.set("state", cursor.getInt(cursor.getColumnIndex("state")) > 0);
                lightObj.set("owned_time", cursor.getLong(cursor.getColumnIndex("owned_time")));
                try {
                    lightObj.saveAllFields(true);
                    ContentValues values = new ContentValues();
                    values.put("synced", true);
                    context.getContentResolver()
                            .update(YanKonProvider.URI_LIGHTS, values, "_id=" + light_id, null);
                } catch (Exception e) {
                    syncResult = false;
                    Log.e(LOG_TAG, Log.getStackTraceString(e));
                }
            } while (cursor.moveToNext());
        }
        return syncResult;
    }

    public static boolean syncLightGroups(Context context, Cursor cursor) {
        KiiUser kiiUser = KiiUser.getCurrentUser();
        if (kiiUser == null) {
            return false;
        }
        boolean syncResult = true;
        KiiBucket bucket = kiiUser.bucket("light_groups");
        if (cursor != null) {
            do {
                boolean synced = cursor.getInt(cursor.getColumnIndex("synced")) > 0;
                if (synced) {
                    continue;
                }
                int group_id = cursor.getInt(cursor.getColumnIndex("_id"));
                String objectID = cursor.getString(cursor.getColumnIndex("objectID"));
                JSONArray childLights = new JSONArray();
                Cursor childCursor = context.getContentResolver()
                        .query(YanKonProvider.URI_LIGHT_GROUP_REL, new String[]{"MAC"},
                                "group_id=" + group_id, null, null);
                if (childCursor != null) {
                    while (childCursor.moveToNext()) {
                        childLights.put(childCursor.getString(0));
                    }
                    childCursor.close();
                }
                KiiObject groupObj;
                groupObj = bucket.object(objectID);
                groupObj.set("name", cursor.getString(cursor.getColumnIndex("name")));
                groupObj.set("group_id", group_id);
                groupObj.set("brightness", cursor.getInt(cursor.getColumnIndex("brightness")));
                groupObj.set("CT", cursor.getInt(cursor.getColumnIndex("CT")));
                groupObj.set("color", cursor.getInt(cursor.getColumnIndex("color")));
                groupObj.set("state", cursor.getInt(cursor.getColumnIndex("state")) > 0);
                groupObj.set("created_time", cursor.getLong(cursor.getColumnIndex("created_time")));
                groupObj.set("lights", childLights);
                try {
                    groupObj.saveAllFields(true);
                    ContentValues values = new ContentValues();
                    values.put("synced", true);
                    context.getContentResolver()
                            .update(YanKonProvider.URI_LIGHT_GROUPS, values, "_id=" + group_id,
                                    null);
                } catch (Exception e) {
                    syncResult = false;
                    Log.e(LOG_TAG, Log.getStackTraceString(e));
                }
            } while (cursor.moveToNext());
        }
        return syncResult;
    }

    public static void asyncSyncLightGroups(final Context context, final int group_id) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Cursor cursor = context.getContentResolver()
                        .query(YanKonProvider.URI_LIGHT_GROUPS, null,
                                group_id > -1 ? ("_id=" + group_id) : null, null, null);
                if (cursor != null) {
                    while (cursor.moveToNext()) {
                        syncLightGroups(context, cursor);
                    }
                    cursor.close();
                }
            }
        };
        new Thread(runnable).start();
    }

    /*
        public static String registLamp(String MAC) {
            String result = null;
            if (!KiiUser.isLoggedIn()) {
                return result;
            }

            KiiServerCodeEntry entry = Kii.serverCodeEntry("registLamp");

            try {
                JSONObject rawArg = new JSONObject();

                rawArg.put("thingID", MAC);
                rawArg.put("batchName", 100);
                KiiServerCodeEntryArgument arg = KiiServerCodeEntryArgument
                        .newArgument(rawArg);

                // Execute the Server Code
                KiiServerCodeExecResult res = entry.execute(arg);

                // Parse the result.
                JSONObject returned = res.getReturnedValue();
                result = returned.getString("returnedValue");
            } catch (Exception e) {

            }
            return result;
        }
    */
    public static String fireLamp(JSONArray lights, int state, int color, int brightness, int CT) {
        String result = null;
        if (!KiiUser.isLoggedIn()) {
            return result;
        }
        KiiServerCodeEntry entry = Kii.serverCodeEntry("fireLamp");
        Log.e(LOG_TAG, "color:" + color);
        long colorL = color & 0x0000000000ffffffL;
        try {
            JSONObject action = new JSONObject();
            action.put("state", state);
            action.put("color", colorL);
            action.put("brightness", brightness);
            action.put("CT", CT);
            JSONObject rawArg = new JSONObject();
            rawArg.put("thing", lights);
            rawArg.put("action", action);
            Log.e(LOG_TAG, "fireLamp:" + rawArg.toString());
            KiiServerCodeEntryArgument arg = KiiServerCodeEntryArgument
                    .newArgument(rawArg);

            // Execute the Server Code
            KiiServerCodeExecResult res = entry.execute(arg);

            // Parse the result.
            JSONObject returned = res.getReturnedValue();
            result = returned.getString("returnedValue");
        } catch (Exception e) {

        }
        return result;
    }

    public static void sync() {
        isSyncing = true;
        downloadLights();
        uploadLights();
        downloadGroups();
        uploadGroups();
        isSyncing = false;
    }

    private static void uploadGroups() {
        KiiBucket bucket = getGroupBucket();
        if (bucket == null) {
            return;
        }
        final Context context = App.getApp();
        Cursor cursor = context.getContentResolver().query(YanKonProvider.URI_LIGHT_GROUPS, null,
                "synced>0 AND deleted=0", null, null);
        if (cursor != null) {
            do {
                String objectId = cursor.getString(cursor.getColumnIndex("objectID"));
                KiiObject groupObj;
                groupObj = bucket.object(objectId);
                groupObj.set("state", cursor.getInt(cursor.getColumnIndex("state")));
                groupObj.set("name", cursor.getString(cursor.getColumnIndex("name")));
                groupObj.set("color", cursor.getInt(cursor.getColumnIndex("color")));
                groupObj.set("brightness", cursor.getInt(cursor.getColumnIndex("brightness")));
                groupObj.set("CT", cursor.getInt(cursor.getColumnIndex("CT")));
                try {
                    groupObj.saveAllFields(true);
                    ContentValues values = new ContentValues();
                    values.put("synced", true);
                    context.getContentResolver()
                            .update(YanKonProvider.URI_LIGHT_GROUPS, values, "objectID=" + objectId,
                                    null);
                } catch (Exception e) {
                    Log.e(LOG_TAG, Log.getStackTraceString(e));
                }
            } while (cursor.moveToNext());
        }
    }

    private static void uploadLights() {
        KiiBucket bucket = getLightBucket();
        if (bucket == null) {
            return;
        }
        final Context context = App.getApp();
        Cursor cursor = context.getContentResolver().query(YanKonProvider.URI_LIGHTS, null,
                "synced>0 AND deleted=0", null, null);
        if (cursor != null) {
            do {
                int light_id = cursor.getInt(cursor.getColumnIndex("_id"));
                KiiObject lightObj;
                String mac = cursor.getString(cursor.getColumnIndex("MAC"));
                lightObj = bucket.object(mac);
                lightObj.set("name", cursor.getString(cursor.getColumnIndex("name")));
                lightObj.set("model", cursor.getString(cursor.getColumnIndex("model")));
                lightObj.set("remote_pwd", cursor.getString(cursor.getColumnIndex("remote_pwd")));
                lightObj.set("admin_pwd", cursor.getString(cursor.getColumnIndex("admin_pwd")));
                lightObj.set("MAC", mac);
                lightObj.set("light_id", light_id);
//                lightObj.set("brightness", cursor.getInt(cursor.getColumnIndex("brightness")));
//                lightObj.set("CT", cursor.getInt(cursor.getColumnIndex("CT")));
//                lightObj.set("color", cursor.getInt(cursor.getColumnIndex("color")));
//                lightObj.set("state", cursor.getInt(cursor.getColumnIndex("state")) > 0);
                lightObj.set("owned_time", cursor.getLong(cursor.getColumnIndex("owned_time")));
                try {
                    lightObj.saveAllFields(true);
                    ContentValues values = new ContentValues();
                    values.put("synced", true);
                    context.getContentResolver()
                            .update(YanKonProvider.URI_LIGHTS, values, "_id=" + light_id, null);
                } catch (Exception e) {
                    Log.e(LOG_TAG, Log.getStackTraceString(e));
                }
            } while (cursor.moveToNext());
        }
    }

    private static void downloadLights() {
        KiiBucket bucket = getLightBucket();
        if (bucket == null) {
            return;
        }
        try {
            KiiQueryResult<KiiObject> result = bucket.query(null);
            List<KiiObject> objList = result.getResult();
            saveRemoteLightRecords(objList);
            while (result.hasNext()) {
                result = result.getNextQueryResult();
                objList = result.getResult();
                saveRemoteLightRecords(objList);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void downloadGroups() {
        KiiBucket bucket = getGroupBucket();
        if (bucket == null) {
            return;
        }
        try {
            KiiQueryResult<KiiObject> result = bucket.query(null);
            List<KiiObject> objList = result.getResult();
            saveRemoteGroupRecords(objList);
            while (result.hasNext()) {
                result = result.getNextQueryResult();
                objList = result.getResult();
                saveRemoteGroupRecords(objList);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void saveRemoteGroupRecords(List<KiiObject> objects) {
        for (KiiObject object : objects) {
            saveRemoteGroupRecord(object);
        }
    }

    private static void saveRemoteGroupRecord(KiiObject object) {
        ContentValues values = new ContentValues();
        values.put("objectID", object.getString("_id"));
        values.put("name", object.getString("name"));
        values.put("state", object.getInt("state"));
        values.put("color", object.getInt("color"));
        values.put("brightness", object.getInt("brightness"));
        values.put("CT", object.getInt("CT"));
        values.put("synced", true);
        Cursor cursor = App.getApp().getContentResolver()
                .query(YanKonProvider.URI_LIGHT_GROUPS, null, "objectID=?",
                        new String[]{object.getString("_id")}, null);
        if (cursor != null && cursor.moveToFirst()) {
            boolean synced = cursor.getInt(cursor.getColumnIndex("synced")) == 1;
            boolean deleted = cursor.getInt(cursor.getColumnIndex("deleted")) == 1;
            if (!deleted) {
                if (synced || Settings.isServerWin()) {
                    App.getApp().getContentResolver()
                            .update(YanKonProvider.URI_LIGHT_GROUPS, values, "objectID=?",
                                    new String[]{object.getString("_id")});
                }
            }
        } else {
            //the remote record does not exist in local storage, save it
            App.getApp().getContentResolver().insert(YanKonProvider.URI_LIGHT_GROUPS, values);
        }
    }

    private static void saveRemoteLightRecords(List<KiiObject> objects) {
        for (KiiObject object : objects) {
            saveRemoteLightRecord(object);
        }
    }

    private static void saveRemoteLightRecord(KiiObject object) {
        ContentValues values = new ContentValues();
        values.put("MAC", object.getString("MAC"));
        values.put("name", object.getString("name"));
        values.put("model", object.getString("model"));
        values.put("remote_pwd", object.getString("remote_pwd"));
        values.put("admin_pwd", object.getString("admin_pwd"));
        values.put("owned_time", object.getLong("owned_time"));
        values.put("synced", true);
        Cursor cursor = App.getApp().getContentResolver()
                .query(YanKonProvider.URI_LIGHTS, null, "MAC=?",
                        new String[]{object.getString("MAC")}, null);
        if (cursor != null && cursor.moveToFirst()) {
            boolean synced = cursor.getInt(cursor.getColumnIndex("synced")) == 1;
            boolean deleted = cursor.getInt(cursor.getColumnIndex("deleted")) == 1;
            if (!deleted) {
                if (synced || Settings.isServerWin()) {
                    App.getApp().getContentResolver()
                            .update(YanKonProvider.URI_LIGHTS, values, "MAC=?",
                                    new String[]{object.getString("MAC")});
                }
            }
        } else {
            //the remote record does not exist in local storage, save it
            App.getApp().getContentResolver().insert(YanKonProvider.URI_LIGHTS, values);
        }
    }

    private static KiiBucket getLightBucket() {
        KiiUser kiiUser = KiiUser.getCurrentUser();
        if (kiiUser == null) {
            return null;
        }
        return kiiUser.bucket("lights");
    }

    private static KiiBucket getGroupBucket() {
        KiiUser kiiUser = KiiUser.getCurrentUser();
        if (kiiUser == null) {
            return null;
        }
        return kiiUser.bucket("groups");
    }

    public static boolean isSyncing() {
        return isSyncing;
    }
}
