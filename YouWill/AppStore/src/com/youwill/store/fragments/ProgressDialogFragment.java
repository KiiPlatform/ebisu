package com.youwill.store.fragments;

import android.app.Dialog;
import android.app.DialogFragment;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.TextUtils;
import com.youwill.store.R;

/**
 * Created by liangyx on 11/11/14.
 */
public class ProgressDialogFragment extends DialogFragment {

    @Override
    public Dialog onCreateDialog(Bundle bundle) {
        String title = getArguments().getString("title", null);
        boolean isCancelable = getArguments().getBoolean("isCancelable", false);

        if (TextUtils.isEmpty(title)) {
            title = getActivity().getString(R.string.loading);
        }

        ProgressDialog mProgressDialog = new ProgressDialog(getActivity(), getTheme());
        mProgressDialog.setCancelable(isCancelable);
        mProgressDialog.setIndeterminate(true);
        mProgressDialog.setTitle(title);
        return mProgressDialog;
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        if (getActivity() instanceof DialogInterface.OnCancelListener) {
            ((DialogInterface.OnCancelListener) getActivity()).onCancel(dialog);
        }
    }

    public static ProgressDialogFragment newInstance(String title, boolean isCancelable) {
        ProgressDialogFragment fragment = new ProgressDialogFragment();
        Bundle args = new Bundle();
        args.putString("title", title);
        args.putBoolean("isCancelable", isCancelable);
        fragment.setArguments(args);
        return fragment;
    }
}
