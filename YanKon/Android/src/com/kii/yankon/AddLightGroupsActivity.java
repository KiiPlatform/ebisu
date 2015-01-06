package com.kii.yankon;

import android.app.Activity;
import android.app.LoaderManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.CheckedTextView;
import android.widget.CursorAdapter;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Toast;

import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.KiiSync;

import java.util.HashSet;
import java.util.UUID;


public class AddLightGroupsActivity extends Activity implements View.OnClickListener, LoaderManager.LoaderCallbacks<Cursor>, AdapterView.OnItemClickListener {

    public static final String EXTRA_GROUP_ID = "group_id";
    public static final String EXTRA_GROUP_NAME = "group_name";

    EditText mGroupNameEdit;
    ListView mList;
    LightsAdapter mAdapter;
    int group_id;
    HashSet<Integer> orgSelectedSet = new HashSet<Integer>();
    HashSet<Integer> selectedSet = new HashSet<Integer>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_light_groups);
        mGroupNameEdit = (EditText) findViewById(R.id.group_name);
        mList = (ListView) findViewById(R.id.group_list);
        findViewById(R.id.group_cancel).setOnClickListener(this);
        findViewById(R.id.group_ok).setOnClickListener(this);

        group_id = getIntent().getIntExtra(EXTRA_GROUP_ID, -1);
        mGroupNameEdit.setText(getIntent().getStringExtra(EXTRA_GROUP_NAME));

        if (group_id >= 0) {
            Cursor cursor = getContentResolver().query(YanKonProvider.URI_LIGHT_GROUP_REL, new String[]{"light_id"}, "group_id=" + group_id, null, null);
            while (cursor.moveToNext()) {
                int lid = cursor.getInt(0);
                Integer integer = Integer.valueOf(lid);
                orgSelectedSet.add(integer);
            }
            cursor.close();
            selectedSet.addAll(orgSelectedSet);
            setTitle(R.string.edit_group);
        } else {
            setTitle(R.string.add_group);
        }
        mAdapter = new LightsAdapter(this);
        mList.setAdapter(mAdapter);
        mList.setOnItemClickListener(this);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.group_cancel:
                finish();
                break;
            case R.id.group_ok:
                save();
                break;
        }
    }

    private void save() {
        String gName = mGroupNameEdit.getText().toString();
        if (TextUtils.isEmpty(gName)) {
            Toast.makeText(this, getString(R.string.empty_group_name), Toast.LENGTH_SHORT).show();
            return;
        }
        ContentResolver cr = getContentResolver();
        ContentValues values = new ContentValues();
        values.put("name", gName);
        if (group_id < 0) {
            values.put("objectID", UUID.randomUUID().toString());
            values.put("brightness", Constants.DEFAULT_BRIGHTNESS);
            values.put("CT", Constants.DEFAULT_CT);
            values.put("color", Constants.DEFAULT_COLOR);
            values.put("created_time", System.currentTimeMillis());
            Uri uri = cr.insert(YanKonProvider.URI_LIGHT_GROUPS, values);
            group_id = Integer.parseInt(uri.getLastPathSegment());
        } else {
            cr.update(YanKonProvider.URI_LIGHT_GROUPS, values, "_id=" + group_id, null);
        }
        Integer[] selArr = selectedSet.toArray(new Integer[0]);
        for (int i = 0; i < selArr.length; i++) {
            Integer integer = selArr[i];
            if (orgSelectedSet.contains(integer)) {
                orgSelectedSet.remove(integer);
                selectedSet.remove(integer);
            }
        }
        for (Integer integer : orgSelectedSet) {
            cr.delete(YanKonProvider.URI_LIGHT_GROUP_REL, "group_id=" + group_id + " AND light_id=" + integer.intValue(), null);
        }
        for (Integer integer : selectedSet) {
            values = new ContentValues();
            values.put("group_id", group_id);
            values.put("light_id", integer.intValue());
            cr.insert(YanKonProvider.URI_LIGHT_GROUP_REL, values);
        }
        KiiSync.asyncSyncLightGroups(this, group_id);
        finish();
    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        return new CursorLoader(this, YanKonProvider.URI_LIGHTS, null, null, null, "owned_time asc");
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void onLoaderReset(Loader<Cursor> loader) {
        mAdapter.swapCursor(null);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        int light_id = (int) id;
        Integer integer = Integer.valueOf(light_id);
        if (selectedSet.contains(integer)) {
            selectedSet.remove(integer);
        } else {
            selectedSet.add(integer);
        }
        mList.invalidateViews();
    }


    class LightsAdapter extends CursorAdapter {
        public LightsAdapter(Context context) {
            super(context, null, 0);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(context).inflate(android.R.layout.simple_list_item_multiple_choice, parent, false);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            String name = cursor.getString(cursor.getColumnIndex("name"));
            CheckedTextView tv = (CheckedTextView) view.findViewById(android.R.id.text1);
            tv.setText(name);
            int light_id = cursor.getInt(cursor.getColumnIndex("_id"));
            tv.setChecked(selectedSet.contains(Integer.valueOf(light_id)));
        }
    }
}
