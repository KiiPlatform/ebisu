package com.kii.yankon.fragments;


import android.app.Fragment;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import com.kii.yankon.R;
import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.KiiSync;
import com.kii.yankon.utils.Settings;

import butterknife.ButterKnife;
import butterknife.InjectView;

/**
 * A simple {@link Fragment} subclass.
 */
public class ProfileFragment extends BaseFragment {

    @InjectView(R.id.email)
    TextView email;

    @InjectView(R.id.log_out_button)
    Button logOutButton;

    @InjectView(R.id.sync_button)
    Button syncButton;

    public ProfileFragment() {
        // Required empty public constructor
    }

    public static ProfileFragment newInstance(int sectionNumber) {
        ProfileFragment fragment = new ProfileFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
    }


    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        View view = inflater.inflate(R.layout.fragment_profile, container, false);
        ButterKnife.inject(this, view);
        email.setText(Settings.getEmail());
        logOutButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Settings.logOut();
                Log.d("ProfileFragment", "after log out, is logged in? " + Settings.isLoggedIn());
                Log.d("ProfileFragment", "after log out, token is " + Settings.getToken());
                LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(new Intent(
                        Constants.INTENT_LOGGED_OUT));
            }
        });
        syncButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new SyncTask().execute();
            }
        });
        return view;
    }


    class SyncTask extends AsyncTask<Void, Void, Void> {

        ProgressDialogFragment dialogFragment;

        @Override
        protected Void doInBackground(Void... params) {
            KiiSync.sync();
            return null;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            dialogFragment = ProgressDialogFragment.newInstance(null, "Syncing...");
            dialogFragment.show(getFragmentManager(), "dialog");
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            super.onPostExecute(aVoid);
            dialogFragment.dismiss();
        }
    }

}
