package com.kii.yankon.fragments;


import android.app.Dialog;
import android.app.DialogFragment;
import android.app.ProgressDialog;
import android.os.Bundle;


public class ProgressDialogFragment extends DialogFragment {

    private static final String ARG_TITLE = "title";
    private static final String ARG_MSG = "msg";

    private String mTitle;
    private String mMessage;

    public static ProgressDialogFragment newInstance(String title, String msg) {
        ProgressDialogFragment fragment = new ProgressDialogFragment();
        Bundle args = new Bundle();
        args.putString(ARG_TITLE, title);
        args.putString(ARG_MSG, msg);
        fragment.setArguments(args);
        return fragment;
    }

    public ProgressDialogFragment() {
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            mTitle = getArguments().getString(ARG_TITLE);
            mMessage = getArguments().getString(ARG_MSG);
        }
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        ProgressDialog dialog = new ProgressDialog(getActivity());
        dialog.setMessage(mMessage);
        dialog.setTitle(mTitle);
        dialog.setCancelable(true);
        return dialog;
    }
}
