package com.youwill.store.fragments;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.view.AppGridAdapter;

import android.app.Fragment;
import android.app.LoaderManager;
import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CursorAdapter;
import android.widget.GridView;

/**
 * Created by tian on 14/10/21:下午10:49.
 */
public class SearchFragment extends Fragment
        implements View.OnClickListener, LoaderManager.LoaderCallbacks<Cursor> {

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_search, container, false);
        initView(view);
        return view;
    }

    AppGridAdapter mAdapter;

    private GridView mGrid;

    private void initView(View view) {
        mGrid = (GridView) view.findViewById(R.id.search_result_grid);
        mAdapter = new AppGridAdapter(getActivity(), null,
                CursorAdapter.FLAG_REGISTER_CONTENT_OBSERVER);
        mGrid.setAdapter(mAdapter);

    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        String keyword = args.getString("keyword");
        String select = TextUtils.isEmpty(keyword) ? null
                : YouWill.Application.SEARCH_FIELD + " LIKE '%" + keyword + "%'";
        LogUtils.d("onCreateLoader, select is " + select);
        return new CursorLoader(getActivity(), YouWill.Application.CONTENT_URI, null, select, null,
                null);
    }

    public void beginSearch(String keyword) {
        Bundle args = new Bundle();
        args.putString("keyword", keyword);
        getLoaderManager().restartLoader(0, args, this);
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void onLoaderReset(Loader<Cursor> loader) {
        mAdapter.swapCursor(null);

    }

    @Override
    public void onClick(View v) {

    }
}
