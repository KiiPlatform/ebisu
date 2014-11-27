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
import android.widget.TextView;

import com.kii.yankon.ColorPickerActivity;
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
            case R.id.action_add:
                startActivityForResult(new Intent(getActivity(), ColorPickerActivity.class), REQUEST_COLOR);
                return true;
        }
        return super.onOptionsItemSelected(item);
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
        return new CursorLoader(getActivity(), YanKonProvider.URI_COLORS, null, null, null, null);
    }

    class ColorAdapter extends CursorAdapter {

        public ColorAdapter(Context context) {
            super(context, null , 0);
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
