package com.kii.yankon.fragments;

import com.kii.yankon.R;

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
    public void onResume() {
        super.onResume();
        getPreferenceScreen().getSharedPreferences()
                .registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        getPreferenceScreen().getSharedPreferences()
                .unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.preferences);
        mPref = PreferenceManager.getDefaultSharedPreferences(getActivity());
        mSyncConfigPref = (ListPreference) findPreference("win_policy");
        setupSyncPolicy();
    }

    private void setupSyncPolicy() {
        int winPolicyIndex = 0;
        try {
            winPolicyIndex = Integer.parseInt(mSyncConfigPref.getValue());
        } catch (NumberFormatException e) {
            e.printStackTrace();
        }
        mSyncConfigPref.setSummary(
                getResources().getStringArray(R.array.sync_policies)[winPolicyIndex]);
    }


    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        setupSyncPolicy();
    }
}
