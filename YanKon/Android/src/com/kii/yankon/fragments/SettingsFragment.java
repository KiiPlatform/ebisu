package com.kii.yankon.fragments;

import com.kii.yankon.MainActivity;
import com.kii.yankon.R;

import android.app.Activity;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;

/**
 * Created by Evan on 14/11/30.
 */
public class SettingsFragment extends PreferenceFragment implements
        OnSharedPreferenceChangeListener {

    public static final String ARG_SECTION_NUMBER = "section_number";

    private ListPreference mSyncConfigPref;

    private ListPreference mDefaultHomePref;

    private SharedPreferences mPref;

    public static SettingsFragment newInstance(int sectionNumber) {
        SettingsFragment fragment = new SettingsFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        MainActivity parentActivity = ((MainActivity) activity);
        parentActivity.onSectionAttached(
                getArguments().getInt(ARG_SECTION_NUMBER));
    }

    @Override
    public void onResume() {
        super.onResume();
        mPref.registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        mPref.unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.preferences);
        mPref = PreferenceManager.getDefaultSharedPreferences(getActivity());
        mSyncConfigPref = (ListPreference) findPreference("win_policy");
        mDefaultHomePref = (ListPreference) findPreference("default_home_page");
        setupSyncPolicy();
    }

    private void setupSyncPolicy() {
        int winPolicyIndex = 0;
        int defaultHomePage = 0;
        try {
            winPolicyIndex = Integer.parseInt(mSyncConfigPref.getValue());
            defaultHomePage = Integer.parseInt(mDefaultHomePref.getValue());
        } catch (NumberFormatException e) {
            e.printStackTrace();
        }
        mSyncConfigPref.setSummary(
                getResources().getStringArray(R.array.sync_policies)[winPolicyIndex]);
        mDefaultHomePref.setSummary(
                getResources().getStringArray(R.array.default_home_page_entries)[defaultHomePage]);
    }


    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        setupSyncPolicy();
    }
}
