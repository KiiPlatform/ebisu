package com.youwill.store.fragments;

import android.app.ListFragment;
import android.app.LoaderManager;
import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.graphics.Color;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.view.AppListAdapter;

/**
 * Created by tian on 14-9-23:下午11:02.
 */
public class UpgradeFragment extends ListFragment implements LoaderManager.LoaderCallbacks<Cursor> {

    AppListAdapter mAdapter;
    TextView emptyTextView;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_list, container, false);
        view.setBackgroundColor(Color.WHITE);
        emptyTextView = (TextView) view.findViewById(android.R.id.empty);
        emptyTextView.setText(getActivity().getString(R.string.no_upgrade));
        return view;
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new AppListAdapter(getActivity(), null, AppListAdapter.TYPE_UPGRADE);
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int i, Bundle bundle) {
        return new CursorLoader(getActivity(), YouWill.Upgrade.CONTENT_URI, null, null, null, null);
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