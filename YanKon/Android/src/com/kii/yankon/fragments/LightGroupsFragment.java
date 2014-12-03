package com.kii.yankon.fragments;

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

import com.kii.yankon.AddLightGroupsActivity;
import com.kii.yankon.R;
import com.kii.yankon.providers.YanKonProvider;

/**
 * Created by Evan on 14/11/26.
 */
public class LightGroupsFragment extends BaseListFragment {

    public static LightGroupsFragment newInstance(int sectionNumber) {
        LightGroupsFragment fragment = new LightGroupsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_add:
                startActivity(new Intent(getActivity(), AddLightGroupsActivity.class));
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new GroupsAdapter(getActivity());
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        return new CursorLoader(getActivity(), YanKonProvider.URI_LIGHT_GROUPS, null, null, null, "created_time asc");
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
        super.onLoadFinished(loader, cursor);
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        Cursor cursor = (Cursor) mAdapter.getItem(position);
        String name = cursor.getString(cursor.getColumnIndex("name"));

    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        Cursor cursor = (Cursor) mAdapter.getItem(info.position);
        String name = cursor.getString(cursor.getColumnIndex("name"));
        menu.setHeaderTitle(name);
        menu.add(0, MENU_EDIT, 0, R.string.menu_edit);
        menu.add(0, MENU_DELETE, 0, R.string.menu_delete);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        super.onContextItemSelected(item);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        switch (item.getItemId()) {
            case MENU_EDIT: {
                Cursor cursor = (Cursor) mAdapter.getItem(info.position);
                int cid = cursor.getInt(cursor.getColumnIndex("_id"));
                String name = cursor.getString(cursor.getColumnIndex("name"));
                Intent intent = new Intent(getActivity(), AddLightGroupsActivity.class);
                intent.putExtra(AddLightGroupsActivity.EXTRA_GROUP_NAME, name);
                intent.putExtra(AddLightGroupsActivity.EXTRA_GROUP_ID, cid);
                startActivity(intent);
            }
            break;
            case MENU_DELETE: {
                Cursor cursor = (Cursor) mAdapter.getItem(info.position);
                int cid = cursor.getInt(cursor.getColumnIndex("_id"));
                getActivity().getContentResolver().delete(YanKonProvider.URI_LIGHT_GROUPS, "_id=" + cid, null);
            }
            break;
        }
        return true;
    }

    class GroupsAdapter extends CursorAdapter {
        public GroupsAdapter(Context context) {
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
            int num = cursor.getInt(cursor.getColumnIndex("num"));
            tv = (TextView) view.findViewById(android.R.id.text2);
            tv.setText(context.getString(R.string.group_num_format, num));
            View icon = view.findViewById(R.id.light_icon);
            icon.setBackgroundResource(R.drawable.light_groups);
            int on_num = cursor.getInt(cursor.getColumnIndex("on_num"));
            Switch light_switch = (Switch) view.findViewById(R.id.light_switch);
            light_switch.setChecked(on_num == num);
        }
    }
}
