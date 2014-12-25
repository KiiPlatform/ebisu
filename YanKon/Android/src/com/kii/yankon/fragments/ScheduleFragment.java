package com.kii.yankon.fragments;

import android.content.ContentValues;
import android.content.Context;
import android.content.CursorLoader;
import android.content.Intent;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.CursorAdapter;
import android.widget.ListView;
import android.widget.Switch;
import android.widget.TextView;

import com.kii.yankon.AddScheduleActivity;
import com.kii.yankon.R;
import com.kii.yankon.providers.YanKonProvider;

/**
 * Created by Evan on 14/11/26.
 */
public class ScheduleFragment extends BaseListFragment {

    public static ScheduleFragment newInstance(int sectionNumber) {
        ScheduleFragment fragment = new ScheduleFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_add:
                startActivity(new Intent(getActivity(), AddScheduleActivity.class));
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new ScheduleAdapter(getActivity());
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        return new CursorLoader(getActivity(), YanKonProvider.URI_SCHEDULE, null, null, null, "created_time asc");
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
        super.onLoadFinished(loader, cursor);
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        Intent intent = new Intent(getActivity(), AddScheduleActivity.class);
        intent.putExtra(AddScheduleActivity.EXTRA_SCHEDULE_ID, (int) id);
        startActivity(intent);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        Cursor cursor = (Cursor) mAdapter.getItem(info.position);
        String name = cursor.getString(cursor.getColumnIndex("name"));
        menu.setHeaderTitle(name);
//        menu.add(0, MENU_EDIT, 0, R.string.menu_edit);
        menu.add(0, MENU_DELETE, 0, R.string.menu_delete);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        super.onContextItemSelected(item);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        switch (item.getItemId()) {
//            case MENU_EDIT: {
//                Cursor cursor = (Cursor) mAdapter.getItem(info.position);
//                int cid = cursor.getInt(cursor.getColumnIndex("_id"));
//                String name = cursor.getString(cursor.getColumnIndex("name"));
//                Intent intent = new Intent(getActivity(), AddLightGroupsActivity.class);
//                intent.putExtra(AddLightGroupsActivity.EXTRA_GROUP_NAME, name);
//                intent.putExtra(AddLightGroupsActivity.EXTRA_GROUP_ID, cid);
//                startActivity(intent);
//            }
//            break;
            case MENU_DELETE: {
                Cursor cursor = (Cursor) mAdapter.getItem(info.position);
                int cid = cursor.getInt(cursor.getColumnIndex("_id"));
                getActivity().getContentResolver().delete(YanKonProvider.URI_SCHEDULE, "_id=" + cid, null);
            }
            break;
        }
        return true;
    }

    class ScheduleAdapter extends CursorAdapter {
        public ScheduleAdapter(Context context) {
            super(context, null, 0);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(context).inflate(R.layout.light_item, parent, false);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            String name = cursor.getString(cursor.getColumnIndex("name"));
            TextView tv = (TextView) view.findViewById(android.R.id.text1);
            tv.setText(name);
            tv = (TextView) view.findViewById(android.R.id.text2);

            View icon = view.findViewById(R.id.light_icon);
            icon.setBackgroundResource(R.drawable.schedules);
            final Switch light_switch = (Switch) view.findViewById(R.id.light_switch);
            final boolean state = cursor.getInt(cursor.getColumnIndex("enabled")) > 0;
            final int schedule_id = cursor.getInt(cursor.getColumnIndex("_id"));
            light_switch.setChecked(state);
            light_switch.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    light_switch.setChecked(!state);
                    ContentValues values = new ContentValues();
                    values.put("enabled", !state);
                    values.put("synced", false);
                    getActivity().getContentResolver().update(YanKonProvider.URI_SCHEDULE, values, "_id=" + schedule_id, null);
//                    Utils.controlGroup(getActivity(), group_id, true);
                }
            });
        }
    }
}
