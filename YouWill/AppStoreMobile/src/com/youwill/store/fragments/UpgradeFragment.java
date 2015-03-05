package com.youwill.store.fragments;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.view.AppListAdapter;

import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * Created by tian on 14-9-23:下午11:02.
 */
public class UpgradeFragment extends BaseAppListFragment {

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
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
    public void onResume() {
        super.onResume();
        getLoaderManager().restartLoader(getClass().hashCode(), null, this);

    }

    @Override
    public Loader<Cursor> onCreateLoader(int i, Bundle bundle) {
        return new CursorLoader(getActivity(), YouWill.Upgrade.CONTENT_URI, null, null, null, null);
    }
}