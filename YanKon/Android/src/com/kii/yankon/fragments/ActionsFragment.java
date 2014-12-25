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
import android.widget.TextView;

import com.kii.yankon.R;
import com.kii.yankon.providers.YanKonProvider;

/**
 * Created by Evan on 14/11/26.
 */
public class ActionsFragment extends BaseListFragment {


    private static final int REQUEST_ACTION = 0x1001;

    public static ActionsFragment newInstance(int sectionNumber) {
        ActionsFragment fragment = new ActionsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_add: {
                currentEditId = -1;
                showEditAction(null);
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    void showEditAction(String text) {
        FragmentTransaction ft = getActivity().getFragmentManager().beginTransaction();
        Fragment prev = getActivity().getFragmentManager().findFragmentByTag("dialog");
        if (prev != null) {
            ft.remove(prev);
        }
        ft.addToBackStack(null);
        InputDialogFragment newFragment = InputDialogFragment.newInstance(
                currentEditId < 0 ? getActivity().getString(R.string.add_action) : getActivity().getString(R.string.edit_action),
                text,
                getActivity().getString(R.string.input_action_name));
        newFragment.setTargetFragment(this, REQUEST_ACTION);
        newFragment.show(getFragmentManager(), "dialog");
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != Activity.RESULT_OK) {
            return;
        }
        switch (requestCode) {
            case REQUEST_ACTION: {
                String name = data.getStringExtra(InputDialogFragment.ARG_TEXT);
                ContentValues values = new ContentValues();
                values.put("name", name);
                if (currentEditId == -1) {
                    values.put("created_time", System.currentTimeMillis());
                    getActivity().getContentResolver().insert(YanKonProvider.URI_ACTIONS, values);
                } else {
                    getActivity().getContentResolver().update(YanKonProvider.URI_ACTIONS, values, "_id=" + currentEditId, null);
                }
            }
            break;
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new ActionAdapter(getActivity());
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        return new CursorLoader(getActivity(), YanKonProvider.URI_ACTIONS, null, null, null, "created_time asc");
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
                getActivity().getContentResolver().delete(YanKonProvider.URI_ACTIONS, "_id=" + cid, null);
            }
            break;
        }
        return true;
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        Cursor cursor = (Cursor) mAdapter.getItem(position);
        String name = cursor.getString(cursor.getColumnIndex("name"));
        currentEditId = cursor.getInt(cursor.getColumnIndex("_id"));
        showEditAction(name);
    }

    class ActionAdapter extends CursorAdapter {

        public ActionAdapter(Context context) {
            super(context, null, 0);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(context).inflate(android.R.layout.simple_list_item_1, parent, false);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            String name = cursor.getString(cursor.getColumnIndex("name"));
            TextView tv = (TextView) view.findViewById(android.R.id.text1);
            tv.setText(name);
        }
    }
}
