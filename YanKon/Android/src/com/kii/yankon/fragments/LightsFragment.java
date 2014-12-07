package com.kii.yankon.fragments;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentTransaction;
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

import com.kii.yankon.AddLightsActivity;
import com.kii.yankon.LightInfoActivity;
import com.kii.yankon.R;
import com.kii.yankon.providers.YanKonProvider;

/**
 * Created by Evan on 14/11/26.
 */
public class LightsFragment extends BaseListFragment {

    private static boolean isFirstLaunch = true;

    private static final int REQUEST_EDIT_NAME = 0x2001;

    public static LightsFragment newInstance(int sectionNumber) {
        LightsFragment fragment = new LightsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_add:
                startActivity(new Intent(getActivity(), AddLightsActivity.class));
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new LightsAdapter(getActivity());
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        return new CursorLoader(getActivity(), YanKonProvider.URI_LIGHTS, null, "connected OR is_mine", null, "owned_time asc");
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
        super.onLoadFinished(loader, cursor);
        if (isFirstLaunch && cursor.getCount() == 0) {
            isFirstLaunch = false;
            startActivity(new Intent(getActivity(), AddLightsActivity.class));
        }
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        Cursor cursor = (Cursor) mAdapter.getItem(info.position);
        String name = cursor.getString(cursor.getColumnIndex("name"));
        menu.setHeaderTitle(name);
        menu.add(0, MENU_EDIT, 0, R.string.menu_edit_name);
        menu.add(0, MENU_DELETE, 0, R.string.menu_delete);
    }


    void showEditAction(String text) {
        FragmentTransaction ft = getActivity().getFragmentManager().beginTransaction();
        Fragment prev = getActivity().getFragmentManager().findFragmentByTag("dialog");
        if (prev != null) {
            ft.remove(prev);
        }
        ft.addToBackStack(null);
        InputDialogFragment newFragment = InputDialogFragment.newInstance(
                getActivity().getString(R.string.edit_light_name),
                text,
                null);
        newFragment.setTargetFragment(this, REQUEST_EDIT_NAME);
        newFragment.show(getFragmentManager(), "dialog");
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        super.onContextItemSelected(item);
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        switch (item.getItemId()) {
            case MENU_EDIT: {
                Cursor cursor = (Cursor) mAdapter.getItem(info.position);
                String name = cursor.getString(cursor.getColumnIndex("name"));
                currentEditId = cursor.getInt(cursor.getColumnIndex("_id"));
                showEditAction(name);
            }
            break;
            case MENU_DELETE: {
                Cursor cursor = (Cursor) mAdapter.getItem(info.position);
                int cid = cursor.getInt(cursor.getColumnIndex("_id"));
                getActivity().getContentResolver().delete(YanKonProvider.URI_LIGHTS, "_id=" + cid, null);
            }
            break;
        }
        return true;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != Activity.RESULT_OK) {
            return;
        }
        switch (requestCode) {
            case REQUEST_EDIT_NAME:
                String name = data.getStringExtra(InputDialogFragment.ARG_TEXT);
                ContentValues values = new ContentValues();
                values.put("name", name);
                if (currentEditId > -1) {
                    getActivity().getContentResolver().update(YanKonProvider.URI_LIGHTS, values, "_id=" + currentEditId, null);
                }
                break;
        }
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        Cursor cursor = (Cursor) mAdapter.getItem(position);
        String name = cursor.getString(cursor.getColumnIndex("name"));
        Intent intent = new Intent(getActivity(), LightInfoActivity.class);
        intent.putExtra(LightInfoActivity.EXTRA_LIGHT_ID, (int) id);
        intent.putExtra(LightInfoActivity.EXTRA_NAME, name);
        startActivity(intent);
    }

    class LightsAdapter extends CursorAdapter {
        public LightsAdapter(Context context) {
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
            String modelName = cursor.getString(cursor.getColumnIndex("m_name"));
            tv = (TextView) view.findViewById(android.R.id.text2);
            tv.setText(context.getString(R.string.light_model_format, modelName));
            View icon = view.findViewById(R.id.light_icon);
            boolean connected = cursor.getInt(cursor.getColumnIndex("connected")) > 0;
            if (connected) {
                icon.setBackgroundResource(R.drawable.light_on);
            } else {
                icon.setBackgroundResource(R.drawable.lights_off);
            }
            Switch light_switch = (Switch) view.findViewById(R.id.light_switch);
            light_switch.setChecked(connected);
        }
    }
}
