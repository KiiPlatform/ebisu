package com.youwill.store.fragments;

import android.app.Fragment;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioButton;

import com.youwill.store.R;
import com.youwill.store.utils.Settings;

/**
 * Created by tian on 14-9-23:下午11:01.
 */
public class CategoriesFragment extends Fragment implements View.OnClickListener {

    private static final String KEY_LAST_CATE = "last_category";
    RadioButton [] cates = new RadioButton[4];
    int current_cate = -1;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_cate, container, false);
        initView(view);
        return view;
    }

    private void initView(View view) {
        cates[0] = (RadioButton) view.findViewById(R.id.cate_btn_1);
        cates[1] = (RadioButton) view.findViewById(R.id.cate_btn_2);
        cates[2] = (RadioButton) view.findViewById(R.id.cate_btn_3);
        cates[3] = (RadioButton) view.findViewById(R.id.cate_btn_4);

        for (RadioButton btn : cates) {
            btn.setOnClickListener(this);
        }
        int last_cate = Settings.getPrefs(getActivity()).getInt(KEY_LAST_CATE, 0);
        if (last_cate < 0 || last_cate > 3) {
            last_cate = 0;
        }
        cates[last_cate].setChecked(true);
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
        SharedPreferences pref = Settings.getPrefs(getActivity());
        SharedPreferences.Editor editor = pref.edit();
        editor.putInt(KEY_LAST_CATE, current_cate);
        editor.commit();
    }
}
