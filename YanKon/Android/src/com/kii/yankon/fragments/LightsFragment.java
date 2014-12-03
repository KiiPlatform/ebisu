package com.kii.yankon.fragments;

import android.content.Context;
import android.content.CursorLoader;
import android.content.Intent;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CursorAdapter;
import android.widget.Switch;
import android.widget.TextView;

import com.kii.yankon.AddLightsActivity;
import com.kii.yankon.R;
import com.kii.yankon.providers.YanKonProvider;

/**
 * Created by Evan on 14/11/26.
 */
public class LightsFragment extends BaseListFragment {

    private static boolean isFirstLaunch = true;

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
