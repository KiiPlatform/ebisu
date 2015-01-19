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

import com.kii.yankon.App;
import com.kii.yankon.R;
import com.kii.yankon.activities.AddLights2Activity;
import com.kii.yankon.activities.LightInfoActivity;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.Utils;

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
                startActivity(new Intent(getActivity(), AddLights2Activity.class));
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
        //"connected OR is_mine"
        return new CursorLoader(getActivity(), YanKonProvider.URI_LIGHTS, null, "deleted=0", null,
                "owned_time asc");
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
        super.onLoadFinished(loader, cursor);
        if (isFirstLaunch) {
            isFirstLaunch = false;
            int num = 0;
            Cursor c = getActivity().getContentResolver()
                    .query(YanKonProvider.URI_LIGHTS, new String[]{"_id"}, null, null, null);
            if (c != null) {
                num = c.getCount();
                c.close();
            }
            if (num == 0) {
                startActivity(new Intent(getActivity(), AddLights2Activity.class));
            }
        }
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v,
            ContextMenu.ContextMenuInfo menuInfo) {
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
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item
                .getMenuInfo();
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
                ContentValues cv = new ContentValues(1);
                cv.put("deleted", 1);
                App.getApp().getContentResolver().update(YanKonProvider.URI_LIGHTS, cv, "_id=?",
                        new String[]{Integer.toString(cid)});
//                getActivity().getContentResolver().delete(YanKonProvider.URI_LIGHTS, "_id=" + cid, null);
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
                    getActivity().getContentResolver()
                            .update(YanKonProvider.URI_LIGHTS, values, "_id=" + currentEditId,
                                    null);
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
            String modelName = cursor.getString(cursor.getColumnIndex("model"));
            tv = (TextView) view.findViewById(android.R.id.text2);
            tv.setText(context.getString(R.string.light_model_format, modelName));
            View icon = view.findViewById(R.id.light_icon);
            final boolean state = cursor.getInt(cursor.getColumnIndex("state")) > 0;
            final int light_id = cursor.getInt(cursor.getColumnIndex("_id"));
            if (state) {
                icon.setBackgroundResource(R.drawable.light_on);
            } else {
                icon.setBackgroundResource(R.drawable.lights_off);
            }
            final Switch light_switch = (Switch) view.findViewById(R.id.light_switch);
            light_switch.setChecked(state);
            light_switch.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    light_switch.setChecked(!state);
                    ContentValues values = new ContentValues();
                    values.put("state", !state);
                    values.put("synced", false);
                    getActivity().getContentResolver()
                            .update(YanKonProvider.URI_LIGHTS, values, "_id=" + light_id, null);
                    Utils.controlLight(getActivity(), light_id, true);
                }
            });
        }
    }
}
