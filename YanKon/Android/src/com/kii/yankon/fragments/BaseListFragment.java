package com.kii.yankon.fragments;

import android.app.Activity;
import android.app.ListFragment;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;

import com.kii.yankon.MainActivity;
import com.kii.yankon.R;

/**
 * Created by Evan on 14/11/26.
 */
public class BaseListFragment extends ListFragment {
    /**
     * The fragment argument representing the section number for this
     * fragment.
     */
    public static final String ARG_SECTION_NUMBER = "section_number";


    protected MainActivity parentActivity;

    /**
     * Returns a new instance of this fragment for the given section number.
     */
    public static BaseListFragment newInstance(int sectionNumber) {
        BaseListFragment fragment = new BaseListFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    public BaseListFragment() {
        setHasOptionsMenu(true);
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        parentActivity = ((MainActivity) activity);
        parentActivity.onSectionAttached(
                getArguments().getInt(ARG_SECTION_NUMBER));
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        menu.clear();
        inflater.inflate(R.menu.common, menu);
    }
}
