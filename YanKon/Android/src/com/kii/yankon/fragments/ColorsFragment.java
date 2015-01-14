package com.kii.yankon.fragments;

import android.app.Activity;
import android.content.ContentValues;
import android.content.Context;
import android.content.CursorLoader;
import android.content.Intent;
import android.content.Loader;
import android.database.Cursor;
import android.graphics.Color;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.CursorAdapter;
import android.widget.ListView;
import android.widget.TextView;

import com.kii.yankon.activities.ColorPickerActivity2;
import com.kii.yankon.R;
import com.kii.yankon.providers.YanKonProvider;

/**
 * Created by Evan on 14/11/26.
 */
public class ColorsFragment extends BaseListFragment {


    private static final int REQUEST_COLOR = 0x1001;

    public static ColorsFragment newInstance(int sectionNumber) {
        ColorsFragment fragment = new ColorsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_add: {
                Intent intent = new Intent(getActivity(), ColorPickerActivity2.class);
                intent.putExtra(ColorPickerActivity2.EXTRA_NAME, "");
                startActivityForResult(intent, REQUEST_COLOR);
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != Activity.RESULT_OK) {
            return;
        }
        switch (requestCode) {
            case REQUEST_COLOR: {
                int id = data.getIntExtra(ColorPickerActivity2.EXTRA_ID, -1);
                int color = data.getIntExtra(ColorPickerActivity2.EXTRA_COLOR, Color.WHITE);
                String name = data.getStringExtra(ColorPickerActivity2.EXTRA_NAME);
                ContentValues values = new ContentValues();
                values.put("name", name);
                values.put("value", color);
                if (id == -1) {
                    values.put("created_time", System.currentTimeMillis());
                    getActivity().getContentResolver().insert(YanKonProvider.URI_COLORS, values);
                } else {
                    getActivity().getContentResolver().update(YanKonProvider.URI_COLORS, values, "_id=" + id, null);
                }
            }
            break;
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new ColorAdapter(getActivity());
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        return new CursorLoader(getActivity(), YanKonProvider.URI_COLORS, null, null, null, "created_time asc");
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        Cursor cursor = (Cursor) mAdapter.getItem(info.position);
        String name = cursor.getString(cursor.getColumnIndex("name"));
        menu.setHeaderTitle(name);
        menu.add(0, MENU_DELETE, 0, R.string.menu_delete);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        super.onContextItemSelected(item);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        switch (item.getItemId()) {
            case MENU_DELETE: {
                Cursor cursor = (Cursor) mAdapter.getItem(info.position);
                int cid = cursor.getInt(cursor.getColumnIndex("_id"));
                getActivity().getContentResolver().delete(YanKonProvider.URI_COLORS, "_id=" + cid, null);
            }
            break;
        }
        return true;
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        Cursor cursor = (Cursor) mAdapter.getItem(position);
        int color = cursor.getInt(cursor.getColumnIndex("value"));
        String name = cursor.getString(cursor.getColumnIndex("name"));
        int cid = cursor.getInt(cursor.getColumnIndex("_id"));
        Intent intent = new Intent(getActivity(), ColorPickerActivity2.class);
        intent.putExtra(ColorPickerActivity2.EXTRA_NAME, name);
        intent.putExtra(ColorPickerActivity2.EXTRA_ID, cid);
        intent.putExtra(ColorPickerActivity2.EXTRA_COLOR, color);
        startActivityForResult(intent, REQUEST_COLOR);
    }

    class ColorAdapter extends CursorAdapter {

        public ColorAdapter(Context context) {
            super(context, null, 0);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(context).inflate(R.layout.color_item, parent, false);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            View cv = view.findViewById(R.id.color);
            int color = cursor.getInt(cursor.getColumnIndex("value"));
            cv.setBackgroundColor(color);
            String name = cursor.getString(cursor.getColumnIndex("name"));
            TextView tv = (TextView) view.findViewById(android.R.id.text1);
            tv.setText(name);
        }
    }
}
