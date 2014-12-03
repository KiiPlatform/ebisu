package com.kii.yankon.fragments;

import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import com.kii.yankon.GuideActivity;
import com.kii.yankon.R;

/**
 * Created by Evan on 14/11/30.
 */
public class SettingsFragment extends BaseListFragment {

    public static SettingsFragment newInstance(int sectionNumber) {
        SettingsFragment fragment = new SettingsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onStart() {
        super.onStart();
        String[] items = getActivity().getResources().getStringArray(R.array.settings);
        ArrayAdapter adapter = new ArrayAdapter(getActivity(),
                android.R.layout.simple_list_item_activated_1,
                android.R.id.text1, items);
        setListAdapter(adapter);
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        switch (position) {
            case 1: {
                Intent intent = new Intent(getActivity(), GuideActivity.class);
                intent.putExtra(GuideActivity.EXTRA_LAUNCH_NEW, false);
                startActivity(intent);
            }
                break;
        }
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
    }
}
