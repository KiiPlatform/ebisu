package com.youwill.store.fragments;

import android.app.Fragment;
import android.app.LoaderManager;
import android.content.CursorLoader;
import android.content.Loader;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CursorAdapter;
import android.widget.GridView;
import android.widget.RadioButton;

import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Settings;
import com.youwill.store.view.AppGridAdapter;

/**
 * Created by Evan on 14-9-23:下午11:01.
 */
public class CategoriesFragment extends Fragment implements View.OnClickListener, LoaderManager.LoaderCallbacks<Cursor> {

    private static final String KEY_LAST_CATE = "last_category";
    RadioButton[] cate_btns = new RadioButton[4];
    int current_cate = -1;
    GridView mGrid;
    AppGridAdapter mAdapter;

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
        mAdapter = new AppGridAdapter(getActivity(), null, CursorAdapter.FLAG_REGISTER_CONTENT_OBSERVER);
        mGrid.setAdapter(mAdapter);
        switchCate(last_cate);
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
}
