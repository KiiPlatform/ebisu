package com.kii.yankon.fragments;

import com.kii.yankon.MainActivity;
import com.kii.yankon.R;

import android.app.Activity;
import android.app.Fragment;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;

/**
 * Created by tian on 14/11/30:下午5:59.
 */
public class BaseFragment extends Fragment implements ProgressDialog.OnCancelListener {

    protected ProgressDialog progressDialog;

    private void initProgressDialog() {
        if (progressDialog == null) {
            progressDialog = new ProgressDialog(getActivity());
        }
        progressDialog.setCancelable(true);
        progressDialog.setOnCancelListener(this);
    }

    protected void showProgress() {
        initProgressDialog();
        progressDialog.show();
    }

    protected void dismissProgress() {
        if (progressDialog != null) {
            progressDialog.dismiss();
            progressDialog = null;
        }
    }

    public static final String ARG_SECTION_NUMBER = "section_number";

    public static final int MENU_DELETE = 1;

    protected MainActivity parentActivity;

    public static BaseFragment newInstance(int sectionNumber) {
        BaseFragment fragment = new BaseFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fragment.setArguments(args);
        return fragment;
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

    @Override
    public void onCancel(DialogInterface dialog) {

    }
}
