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
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CursorAdapter;
import android.widget.ListView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import com.kii.yankon.R;
import com.kii.yankon.activities.AddLights2Activity;
import com.kii.yankon.activities.LightInfoActivity;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.DataHelper;
import com.kii.yankon.utils.Utils;

import java.util.HashSet;

/**
 * Created by Evan on 14/11/26.
 */
public class LightsFragment extends BaseListFragment implements CompoundButton.OnCheckedChangeListener {

    private static boolean isFirstLaunch = true;

    private static final int REQUEST_EDIT_NAME = 0x2001;

    View headerView;

    boolean inMultipleMode = false;

    HashSet<String> mSelectedLights = new HashSet<>();

    public static LightsFragment newInstance(int sectionNumber) {
        LightsFragment fragment = new LightsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        menu.clear();
        inflater.inflate(R.menu.lights, menu);
    }

    @Override
    public void onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        menu.findItem(R.id.action_select_all).setVisible(inMultipleMode);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_multiple: {
                inMultipleMode = !inMultipleMode;
                switchMode();
            }
            return true;
            case R.id.action_add:
                startActivity(new Intent(getActivity(), AddLights2Activity.class));
                return true;
            case R.id.action_select_all: {
                boolean isAllSelected = true;
                for (int i = 0; i < mAdapter.getCount(); i++) {
                    Cursor cursor = (Cursor) mAdapter.getItem(i);
                    String lid = cursor.getString(cursor.getColumnIndex("_id"));
                    if (!mSelectedLights.contains(lid)) {
                        isAllSelected = false;
                        mSelectedLights.add(lid);
                    }
                }
                if (isAllSelected) {
                    mSelectedLights.clear();
                }
                updateHeaderView();
                getListView().invalidateViews();
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        initHeaderView();
    }

    void initHeaderView() {
        ListView lv = getListView();
        View headerViewContainer = View.inflate(getActivity(), R.layout.lights_header, null);
        lv.addHeaderView(headerViewContainer);
        headerView = headerViewContainer.findViewById(R.id.light_item);
        headerView.findViewById(R.id.light_icon).setVisibility(View.GONE);
        TextView tv = (TextView) headerView.findViewById(android.R.id.text1);
        tv.setText(R.string.header_selected_lights);
        updateHeaderView();
        switchMode();
    }

    void updateHeaderView() {
        TextView tv = (TextView) headerView.findViewById(android.R.id.text2);
        tv.setText(getString(R.string.header_lights_amount, mSelectedLights.size()));
    }

    void switchMode() {
        headerView.setVisibility(inMultipleMode ? View.VISIBLE : View.GONE);
        getActivity().invalidateOptionsMenu();
        getListView().invalidateViews();
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
        String name = null;
        if (info.position == 0) {
            if (mSelectedLights.size() == 0) {
                Toast.makeText(getActivity(), R.string.lights_multiple_empty, Toast.LENGTH_SHORT).show();
                return;
            }
            if (mSelectedLights.size() == 1) {
                name = getString(R.string.lights_multiple_title_1light);
            } else {
                name = getString(R.string.lights_multiple_title, mSelectedLights.size());
            }
        } else {
            Cursor cursor = (Cursor) mAdapter.getItem(info.position - 1);
            name = cursor.getString(cursor.getColumnIndex("name"));
            menu.add(0, MENU_EDIT, 0, R.string.menu_edit_name);
        }
        menu.setHeaderTitle(name);
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
        int pos = info.position - 1;
        switch (item.getItemId()) {
            case MENU_EDIT: {
                Cursor cursor = (Cursor) mAdapter.getItem(pos);
                String name = cursor.getString(cursor.getColumnIndex("name"));
                currentEditId = cursor.getInt(cursor.getColumnIndex("_id"));
                showEditAction(name);
            }
            break;
            case MENU_DELETE: {
                if (info.position == 0) {
                    for (String lid : mSelectedLights) {
                        DataHelper.deleteLightById(Integer.parseInt(lid));
                    }
                    mSelectedLights.clear();
                } else {
                    Cursor cursor = (Cursor) mAdapter.getItem(pos);
                    int cid = cursor.getInt(cursor.getColumnIndex("_id"));
                    String mac = cursor.getString(cursor.getColumnIndex("MAC"));
                    DataHelper.deleteLightById(cid);
                    mSelectedLights.remove(mac);
                }
                updateHeaderView();
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
                values.put("synced", false);
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
        String name = null;
        Intent intent = new Intent(getActivity(), LightInfoActivity.class);
        if (position == 0) {
            if (mSelectedLights.size() == 0) {
                Toast.makeText(getActivity(), R.string.lights_multiple_empty, Toast.LENGTH_SHORT).show();
                return;
            }
            String[] lights = mSelectedLights.toArray(new String[mSelectedLights.size()]);
            if (lights.length == 1) {
                Cursor cursor = getActivity().getContentResolver().query(YanKonProvider.URI_LIGHTS,
                        new String[]{"name"},
                        "_id=(?)", new String[]{lights[0]},
                        null);
                if (cursor != null) {
                    if (cursor.moveToFirst()) {
                        name = cursor.getString(0);
                        intent.putExtra(LightInfoActivity.EXTRA_LIGHT_ID, Integer.parseInt(lights[0]));
                    }
                    cursor.close();
                }
            } else {
                name = getString(R.string.lights_multiple_title, mSelectedLights.size());
                intent.putExtra(LightInfoActivity.EXTRA_LIGHTS, lights);
            }
        } else {
            Cursor cursor = (Cursor) mAdapter.getItem(position - 1);
            name = cursor.getString(cursor.getColumnIndex("name"));
            intent.putExtra(LightInfoActivity.EXTRA_LIGHT_ID, (int) id);
        }
        intent.putExtra(LightInfoActivity.EXTRA_NAME, name);
        startActivity(intent);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        String lid = (String) buttonView.getTag();
        if (isChecked) {
            mSelectedLights.add(lid);
        } else {
            mSelectedLights.remove(lid);
        }
        updateHeaderView();
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
            CheckBox checkBox = (CheckBox) view.findViewById(R.id.light_checkbox);
            final boolean state = cursor.getInt(cursor.getColumnIndex("state")) > 0;
            final int light_id = cursor.getInt(cursor.getColumnIndex("_id"));
//            String mac = cursor.getString(cursor.getColumnIndex("MAC"));
            if (inMultipleMode) {
                checkBox.setVisibility(View.VISIBLE);
                icon.setBackgroundResource(0);
                String lid = String.valueOf(light_id);
                checkBox.setTag(lid);
                checkBox.setOnCheckedChangeListener(LightsFragment.this);
                checkBox.setChecked(mSelectedLights.contains(lid));
            } else {
                checkBox.setVisibility(View.GONE);
                if (state) {
                    icon.setBackgroundResource(R.drawable.light_on);
                } else {
                    icon.setBackgroundResource(R.drawable.lights_off);
                }
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
