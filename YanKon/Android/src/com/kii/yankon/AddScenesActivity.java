package com.kii.yankon;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseExpandableListAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ExpandableListView;
import android.widget.TextView;
import android.widget.Toast;

import com.kii.yankon.model.Light;
import com.kii.yankon.model.LightGroup;
import com.kii.yankon.providers.YanKonProvider;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.UUID;

/**
 * Created by Evan on 14/12/8.
 */
public class AddScenesActivity extends Activity implements View.OnClickListener, ExpandableListView.OnChildClickListener {

    public static final String EXTRA_SCENE_ID = "scene_id";
    public static final String EXTRA_SCENE_NAME = "scene_name";

    EditText mSceneNameEdit;
    ExpandableListView mList;
    int scene_id;
    HashSet<String> orgSelectedSet = new HashSet<String>();
    HashSet<String> selectedSet = new HashSet<String>();
    SceneAdapter mAdapter;

    ArrayList<Light> mLights = new ArrayList<Light>();
    ArrayList<LightGroup> mGroups = new ArrayList<LightGroup>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_scenes);
        mSceneNameEdit = (EditText) findViewById(R.id.scene_name);
        mList = (ExpandableListView) findViewById(R.id.scene_list);
        findViewById(R.id.scene_cancel).setOnClickListener(this);
        findViewById(R.id.scene_ok).setOnClickListener(this);

        scene_id = getIntent().getIntExtra(EXTRA_SCENE_ID, -1);
        mSceneNameEdit.setText(getIntent().getStringExtra(EXTRA_SCENE_NAME));
        loadContents();
        mAdapter = new SceneAdapter();
        mList.setAdapter(mAdapter);
        mList.setOnChildClickListener(this);
        mList.expandGroup(0);
        mList.expandGroup(1);
    }

    void loadContents() {
        Cursor c = getContentResolver().query(YanKonProvider.URI_LIGHTS, null, null, null, null);
        while (c.moveToNext()) {
            Light l = new Light();
            l.name = c.getString(c.getColumnIndex("name"));
            l.id = c.getInt(c.getColumnIndex("_id"));
            l.UUID = c.getString(c.getColumnIndex("UUID"));
            l.modelName = c.getString(c.getColumnIndex("m_name"));
            mLights.add(l);
        }
        c.close();
        c = getContentResolver().query(YanKonProvider.URI_LIGHT_GROUPS, null, null, null, null);
        while (c.moveToNext()) {
            LightGroup l = new LightGroup();
            l.name = c.getString(c.getColumnIndex("name"));
            l.id = c.getInt(c.getColumnIndex("_id"));
            l.UUID = c.getString(c.getColumnIndex("UUID"));
            l.num = c.getInt(c.getColumnIndex("num"));
            mGroups.add(l);
        }
        c.close();
        if (scene_id >= 0) {
            c = getContentResolver().query(YanKonProvider.URI_SCENES_DETAIL, null, "scene_id=" + scene_id, null, null);
            while (c.moveToNext()) {
                int light_id = c.getInt(c.getColumnIndex("light_id"));
                if (light_id >= 0) {
                    orgSelectedSet.add("l" + light_id);
                } else {
                    int group_id = c.getInt(c.getColumnIndex("group_id"));
                    orgSelectedSet.add("g" + group_id);
                }
            }
            c.close();
            selectedSet.addAll(orgSelectedSet);
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.scene_cancel:
                finish();
                break;
            case R.id.scene_ok:
                save();
                break;
            case R.id.light_checkbox: {
                String key = (String) v.getTag();
                if (selectedSet.contains(key)) {
                    selectedSet.remove(key);
                } else {
                    selectedSet.add(key);
                }
                mList.invalidateViews();
            }
            break;
        }
    }

    private void save() {
        String gName = mSceneNameEdit.getText().toString();
        if (TextUtils.isEmpty(gName)) {
            Toast.makeText(this, getString(R.string.empty_scene_name), Toast.LENGTH_SHORT).show();
            return;
        }
        ContentResolver cr = getContentResolver();
        ContentValues values = new ContentValues();
        values.put("name", gName);
        if (scene_id < 0) {
            values.put("UUID", UUID.randomUUID().toString());
            values.put("created_time", System.currentTimeMillis());
            Uri uri = cr.insert(YanKonProvider.URI_SCENES, values);
            scene_id = Integer.parseInt(uri.getLastPathSegment());
        } else {
            cr.update(YanKonProvider.URI_SCENES, values, "_id=" + scene_id, null);
        }
        String[] selArr = selectedSet.toArray(new String[0]);
        for (int i = 0; i < selArr.length; i++) {
            String data = selArr[i];
            if (orgSelectedSet.contains(data)) {
                orgSelectedSet.remove(data);
                selectedSet.remove(data);
            }
        }
        for (String data : orgSelectedSet) {
            if (data.length() <= 1)
                continue;
            ;
            String num = data.substring(1);
            int id = Integer.parseInt(num);
            if (data.charAt(0) == 'l') {
                cr.delete(YanKonProvider.URI_SCENES_DETAIL, "scene_id=" + scene_id + " AND light_id=" + id, null);
            } else {
                cr.delete(YanKonProvider.URI_SCENES_DETAIL, "scene_id=" + scene_id + " AND group_id=" + id, null);
            }
        }
        for (String data : selectedSet) {
            if (data.length() <= 1)
                continue;
            ;
            String num = data.substring(1);
            int id = Integer.parseInt(num);
            values = new ContentValues();
            values.put("scene_id", scene_id);
            if (data.charAt(0) == 'l') {
                values.put("light_id", id);
                values.put("group_id", -1);
            } else {
                values.put("light_id", -1);
                values.put("group_id", id);
            }
            cr.insert(YanKonProvider.URI_SCENES_DETAIL, values);
        }
        finish();
    }

    @Override
    public boolean onChildClick(ExpandableListView parent, View v, int groupPosition, int childPosition, long id) {
        return false;
    }

    class SceneAdapter extends BaseExpandableListAdapter {

        public SceneAdapter() {
            super();
        }

        @Override
        public int getGroupCount() {
            return 2;
        }

        @Override
        public int getChildrenCount(int groupPosition) {
            if (groupPosition == 0) {
                return mLights.size();
            }
            return mGroups.size();
        }

        @Override
        public Object getGroup(int groupPosition) {
            return null;
        }

        @Override
        public Object getChild(int groupPosition, int childPosition) {
            return null;
        }

        @Override
        public long getGroupId(int groupPosition) {
            return 0;
        }

        @Override
        public long getChildId(int groupPosition, int childPosition) {
            return 0;
        }

        @Override
        public boolean hasStableIds() {
            return false;
        }

        @Override
        public View getGroupView(int groupPosition, boolean isExpanded, View convertView, ViewGroup parent) {
            TextView view = (TextView) convertView;
            if (view == null) {
                view = (TextView) getLayoutInflater().inflate(android.R.layout.simple_expandable_list_item_1, parent, false);
            }
            view.setText(groupPosition == 0 ? R.string.Lights : R.string.light_groups);
            return view;
        }

        @Override
        public View getChildView(int groupPosition, int childPosition, boolean isLastChild, View convertView, ViewGroup parent) {
            View view = convertView;
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.checkable_item, parent, false);
            }
            TextView tv = (TextView) view.findViewById(android.R.id.text1);
            String name, line2;
            View icon = view.findViewById(R.id.light_icon);
            String key;
            if (groupPosition == 0) {
                name = mLights.get(childPosition).name;
                icon.setBackgroundResource(R.drawable.light_on);
                line2 = getString(R.string.light_model_format, mLights.get(childPosition).modelName);
                key = "l" + mLights.get(childPosition).id;
            } else {
                name = mGroups.get(childPosition).name;
                icon.setBackgroundResource(R.drawable.light_groups);
                line2 = getString(R.string.group_num_format, mGroups.get(childPosition).num);
                key = "g" + mGroups.get(childPosition).id;
            }
            tv.setText(name);
            tv = (TextView) view.findViewById(android.R.id.text2);
            tv.setText(line2);
            CheckBox cb = (CheckBox) view.findViewById(R.id.light_checkbox);
            cb.setOnClickListener(AddScenesActivity.this);
            cb.setChecked(selectedSet.contains(key));
            cb.setTag(key);
            return view;
        }

        @Override
        public boolean isChildSelectable(int groupPosition, int childPosition) {
            return false;
        }
    }
}
