package com.youwill.store.fragments;

import com.kii.payment.PayType;
import com.youwill.store.R;
import com.youwill.store.activities.AppDetailActivity;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.view.AppGridAdapter;

import android.app.Fragment;
import android.app.LoaderManager;
import android.content.CursorLoader;
import android.content.Intent;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.CursorAdapter;
import android.widget.GridView;

/**
 * Created by tian on 14/10/21:下午10:49.
 */
public class SearchFragment extends Fragment
        implements LoaderManager.LoaderCallbacks<Cursor>, AdapterView.OnItemClickListener {

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
        mGrid.setOnItemClickListener(this);

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
        getLoaderManager().restartLoader(getClass().hashCode(), args, this);
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void onLoaderReset(Loader<Cursor> loader) {
        mAdapter.swapCursor(null);
    }

    public void launchPayment(PayType payType) {
        mAdapter.launchPayment(payType);
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mAdapter.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        Cursor cursor = (Cursor) mAdapter.getItem(position);
        if (cursor == null) {
            return;
        }
        String appId = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_ID));
        Intent intent = new Intent(getActivity(), AppDetailActivity.class);
        intent.putExtra(AppDetailActivity.EXTRA_APP_ID, appId);
        int isPurchased = cursor.getInt(cursor.getColumnIndex(YouWill.Purchased.IS_PURCHASED));
        intent.putExtra("is_purchased", isPurchased == 1);
        startActivity(intent);
    }
}
