package com.kii.yankon.fragments;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import com.kii.yankon.R;


public class InputDialogFragment extends DialogFragment {

    private static final String ARG_TITLE = "title";
    public static final String ARG_TEXT = "text";
    private static final String ARG_HINT = "hint";

    private String title, text, hint;

    public static InputDialogFragment newInstance(String title, String text, String hint) {
        InputDialogFragment fragment = new InputDialogFragment();
        Bundle args = new Bundle();
        args.putString(ARG_TITLE, title);
        args.putString(ARG_TEXT, text);
        args.putString(ARG_HINT, hint);
        fragment.setArguments(args);
        return fragment;
    }

    public InputDialogFragment() {
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            title = getArguments().getString(ARG_TITLE);
            text = getArguments().getString(ARG_TEXT);
            hint = getArguments().getString(ARG_HINT);
        }
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        LayoutInflater inflater = LayoutInflater.from(getActivity());
        final View v = inflater.inflate(R.layout.fragment_input_dialog, null);
        final EditText et = (EditText) v.findViewById(R.id.input_edit);
        et.setText(text);
        et.setHint(hint);
        if (!TextUtils.isEmpty(text))
            et.setSelection(text.length());
        return new AlertDialog.Builder(getActivity())
                .setTitle(title)
                .setView(v)
                .setCancelable(true)
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Intent intent = new Intent();
                        intent.putExtra(ARG_TEXT, et.getText().toString());
                        getTargetFragment().onActivityResult(getTargetRequestCode(),
                                Activity.RESULT_OK,
                                intent);
                    }
                })
                .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        getTargetFragment().onActivityResult(getTargetRequestCode(),
                                Activity.RESULT_CANCELED,
                                getActivity().getIntent());
                    }
                }).create();
    }

}
