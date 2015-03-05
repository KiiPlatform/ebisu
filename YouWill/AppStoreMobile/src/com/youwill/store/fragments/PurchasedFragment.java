package com.youwill.store.fragments;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Settings;
import com.youwill.store.view.AppListAdapter;
import com.youwill.store.view.LoginSpan;

import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * Created by tian on 14-9-23:下午11:02.
 */
public class PurchasedFragment extends BaseAppListFragment {

    SpannableString mNotLoggedInEmptyText;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        emptyTextView.setMovementMethod(LinkMovementMethod.getInstance());
        return view;
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new AppListAdapter(getActivity(), null, AppListAdapter.TYPE_PURCHASED);
        setListAdapter(mAdapter);
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
        String not_login = getActivity().getString(R.string.purchased_not_login);
        String login_pattern = getActivity().getString(R.string.login_pattern);
        int pos = not_login.indexOf(login_pattern);
        int length = login_pattern.length();
        mNotLoggedInEmptyText = new SpannableString(not_login);
        LoginSpan loginSpan = new LoginSpan();
        if (pos >= 0) {
            mNotLoggedInEmptyText
                    .setSpan(loginSpan, pos, pos + length, Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (Settings.isLoggedIn(getActivity())) {
            emptyTextView.setText(getActivity().getString(R.string.no_purchased_apps));
        } else {
            emptyTextView.setText(mNotLoggedInEmptyText);
            getLoaderManager().restartLoader(getClass().hashCode(), null, this);
        }
    }

    @Override
    public Loader<Cursor> onCreateLoader(int i, Bundle bundle) {
        return new CursorLoader(getActivity(), YouWill.Purchased.CONTENT_URI, null, null, null,
                null);
    }
}
