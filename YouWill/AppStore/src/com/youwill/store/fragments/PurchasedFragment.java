package com.youwill.store.fragments;

import android.app.Fragment;
import android.app.ListFragment;
import android.app.LoaderManager;
import android.content.Context;
import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CursorAdapter;

import com.youwill.store.providers.YouWill;

/**
 * Created by tian on 14-9-23:下午11:02.
 */
public class PurchasedFragment extends ListFragment implements LoaderManager.LoaderCallbacks<Cursor>{

    PurchasedAppAdapter mAdapter;

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new PurchasedAppAdapter(getActivity(), null, CursorAdapter.FLAG_REGISTER_CONTENT_OBSERVER);
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int i, Bundle bundle) {
        return new CursorLoader(getActivity(), YouWill.Purchased.CONTENT_URI, null, null, null, null);
    }

    @Override
    public void onLoadFinished(Loader<Cursor> objectLoader, Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void onLoaderReset(Loader<Cursor> objectLoader) {
        mAdapter.swapCursor(null);
    }

    class PurchasedAppAdapter extends CursorAdapter {
        public PurchasedAppAdapter(Context context, Cursor c, int flags) {
            super(context, c, flags);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return null;
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {

        }
    }
}
