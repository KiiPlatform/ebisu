package com.kii.yankon.fragments;

import android.content.Intent;
import android.os.Bundle;
import android.view.MenuItem;

import com.kii.yankon.AddLightsActivity;
import com.kii.yankon.R;

/**
 * Created by Evan on 14/11/26.
 */
public class ColorsFragment extends BaseListFragment {


    public static ColorsFragment newInstance(int sectionNumber) {
        ColorsFragment fragment = new ColorsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_add:
                startActivity(new Intent(getActivity(), AddLightsActivity.class));
                return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
