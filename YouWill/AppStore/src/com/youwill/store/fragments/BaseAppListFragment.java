package com.youwill.store.fragments;

import android.app.ListFragment;
import android.app.LoaderManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.Loader;
import android.database.Cursor;
import android.graphics.Color;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.youwill.store.R;
import com.youwill.store.utils.Constants;
import com.youwill.store.view.AppListAdapter;

/**
 * Created by Evan on 14/10/24.
 */
public abstract class BaseAppListFragment extends ListFragment implements LoaderManager.LoaderCallbacks<Cursor> {
    AppListAdapter mAdapter;
    TextView emptyTextView;
    BroadcastReceiver mReciver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            getListView().invalidateViews();
        }
    };

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_list, container, false);
        view.setBackgroundColor(Color.WHITE);
        emptyTextView = (TextView) view.findViewById(android.R.id.empty);
        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        getActivity().registerReceiver(mReciver, new IntentFilter(Constants.INTENT_DOWNLOAD_PROGRESS_CHANGED));
    }

    @Override
    public void onPause() {
        super.onPause();
        getActivity().unregisterReceiver(mReciver);
    }

    @Override
    public void onLoadFinished(Loader<Cursor> objectLoader, Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void onLoaderReset(Loader<Cursor> objectLoader) {
        mAdapter.swapCursor(null);
    }
}
