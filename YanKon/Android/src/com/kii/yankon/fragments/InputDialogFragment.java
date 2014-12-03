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
import android.widget.Toast;

import com.kii.yankon.R;


public class InputDialogFragment extends DialogFragment {

    private static final String ARG_TITLE = "title";
    public static final String ARG_TEXT = "text";
    private static final String ARG_HINT = "hint";

    private String title, text, hint;
    InputDialogInterface mListener;

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
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        if (activity instanceof InputDialogInterface) {
            mListener = (InputDialogInterface) activity;
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mListener = null;
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
                        String data = et.getText().toString();
                        if (TextUtils.isEmpty(data)) {
                            Toast.makeText(getActivity(), getActivity().getString(R.string.input_dialog_empty), Toast.LENGTH_SHORT).show();
                            return;
                        }
                        Intent intent = new Intent();
                        intent.putExtra(ARG_TEXT, data);
                        if (getTargetFragment() != null) {
                            getTargetFragment().onActivityResult(getTargetRequestCode(),
                                    Activity.RESULT_OK,
                                    intent);
                        }
                        if (mListener != null) {
                            mListener.onInputDialogTextDone(data);
                        }
                    }
                })
                .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        if (getTargetFragment() != null) {
                            getTargetFragment().onActivityResult(getTargetRequestCode(),
                                    Activity.RESULT_CANCELED,
                                    getActivity().getIntent());
                        }
                    }
                }).create();
    }

    public static interface InputDialogInterface {
        public void onInputDialogTextDone(String text);
    }

}
