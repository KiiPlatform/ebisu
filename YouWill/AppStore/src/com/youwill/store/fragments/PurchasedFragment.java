package com.youwill.store.fragments;

import android.app.ListFragment;
import android.app.LoaderManager;
import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Settings;
import com.youwill.store.view.AppListAdapter;
import com.youwill.store.view.LoginSpan;

/**
 * Created by tian on 14-9-23:下午11:02.
 */
public class PurchasedFragment extends ListFragment implements LoaderManager.LoaderCallbacks<Cursor> {

    AppListAdapter mAdapter;
    SpannableString mNotLoggedInEmptyText;
    TextView emptyTextView;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_list, container, false);
        view.setBackgroundColor(Color.WHITE);
        emptyTextView = (TextView) view.findViewById(android.R.id.empty);
        emptyTextView.setMovementMethod(LinkMovementMethod.getInstance());
        return view;
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new AppListAdapter(getActivity(), null, AppListAdapter.TYPE_PURCHASED);
        setListAdapter(mAdapter);
        getListView().setDivider(new ColorDrawable(Color.TRANSPARENT));
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
        String not_login = getActivity().getString(R.string.purchased_not_login);
        String login_pattern = getActivity().getString(R.string.login_pattern);
        int pos = not_login.indexOf(login_pattern);
        int length = login_pattern.length();
        mNotLoggedInEmptyText = new SpannableString(not_login);
        LoginSpan loginSpan = new LoginSpan();
        if (pos >= 0) {
            mNotLoggedInEmptyText.setSpan(loginSpan, pos, pos + length, Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (Settings.isLoggedIn(getActivity())) {
            emptyTextView.setText(getActivity().getString(R.string.no_purchased_apps));
        } else {
            emptyTextView.setText(mNotLoggedInEmptyText);
        }
    }

    @Override
    public Loader<Cursor> onCreateLoader(int i, Bundle bundle) {
        //TODO Next 3 lines are for testing only, should remove later
        if (!Settings.isLoggedIn(getActivity())) {
            return new CursorLoader(getActivity(), YouWill.Purchased.CONTENT_URI, null, "1=0", null, null);
        }
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
}
