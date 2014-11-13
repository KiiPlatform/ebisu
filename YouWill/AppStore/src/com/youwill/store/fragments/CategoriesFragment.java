package com.youwill.store.fragments;

import com.youwill.store.R;
import com.youwill.store.activities.AppDetailActivity;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Constants;
import com.youwill.store.utils.Settings;
import com.youwill.store.view.AppGridAdapter;

import android.app.Fragment;
import android.app.LoaderManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.CursorLoader;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.Loader;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.content.LocalBroadcastManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.GridView;
import android.widget.RadioButton;


/**
 * Created by Evan on 14-9-23:下午11:01.
 */
public class CategoriesFragment extends Fragment implements View.OnClickListener, LoaderManager.LoaderCallbacks<Cursor>, AdapterView.OnItemClickListener {

    private static final String KEY_LAST_CATE = "last_category";
    RadioButton[] cate_btns = new RadioButton[4];
    int current_cate = -1;
    GridView mGrid;
    AppGridAdapter mAdapter;

    BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (mGrid != null)
                mGrid.invalidateViews();
        }
    };

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_cate, container, false);
        initView(view);
        return view;
    }

    private void initView(View view) {
        cate_btns[0] = (RadioButton) view.findViewById(R.id.cate_btn_1);
        cate_btns[1] = (RadioButton) view.findViewById(R.id.cate_btn_2);
        cate_btns[2] = (RadioButton) view.findViewById(R.id.cate_btn_3);
        cate_btns[3] = (RadioButton) view.findViewById(R.id.cate_btn_4);

        for (RadioButton btn : cate_btns) {
            btn.setOnClickListener(this);
        }
        int last_cate = Settings.getPrefs(getActivity()).getInt(KEY_LAST_CATE, 0);
        if (last_cate < 0 || last_cate > 3) {
            last_cate = 0;
        }
        cate_btns[last_cate].setChecked(true);
        mGrid = (GridView) view.findViewById(R.id.cate_grid);
        mAdapter = new AppGridAdapter(getActivity(), null, 0);
        mGrid.setAdapter(mAdapter);
        mGrid.setOnItemClickListener(this);
        switchCate(last_cate);
    }

    @Override
    public void onResume() {
        super.onResume();
        LocalBroadcastManager.getInstance(getActivity()).registerReceiver(mReceiver, new IntentFilter(Constants.INTENT_DOWNLOAD_PROGRESS_CHANGED));
    }

    @Override
    public void onPause() {
        super.onPause();
        LocalBroadcastManager.getInstance(getActivity()).unregisterReceiver(mReceiver);
    }


    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.cate_btn_1:
                switchCate(0);
                break;
            case R.id.cate_btn_2:
                switchCate(1);
                break;
            case R.id.cate_btn_3:
                switchCate(2);
                break;
            case R.id.cate_btn_4:
                switchCate(3);
                break;
            default:
                break;
        }
    }

    protected void switchCate(int index) {
        if (index == current_cate) {
            return;
        }
        current_cate = index;
        cate_btns[index].setChecked(true);
        SharedPreferences pref = Settings.getPrefs(getActivity());
        SharedPreferences.Editor editor = pref.edit();
        editor.putInt(KEY_LAST_CATE, current_cate);
        editor.commit();
        getLoaderManager().restartLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int i, Bundle bundle) {
        String select = YouWill.Application.AGE_CATEGORY + "=" + current_cate;
        return new CursorLoader(getActivity(), YouWill.Application.CONTENT_URI, null, select, null, null);
    }

    @Override
    public void onLoadFinished(Loader<Cursor> objectLoader, Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void onLoaderReset(Loader<Cursor> objectLoader) {
        mAdapter.swapCursor(null);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        Cursor cursor = (Cursor)mAdapter.getItem(position);
        if (cursor == null) {
            return;
        }
        String appId = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_ID));
        Intent intent = new Intent(getActivity(), AppDetailActivity.class);
        intent.putExtra(AppDetailActivity.EXTRA_APP_ID, appId);
        startActivity(intent);
    }
}
