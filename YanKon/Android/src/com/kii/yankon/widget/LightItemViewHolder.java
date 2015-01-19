package com.kii.yankon.widget;

import android.view.View;
import android.widget.CheckBox;
import android.widget.Switch;
import android.widget.TextView;

import com.kii.yankon.R;

/**
 * Created by Evan on 15/1/20.
 */
public class LightItemViewHolder {
    public View icon;
    public CheckBox checkBox;
    public Switch switchButton;
    public TextView textView1;
    public TextView textView2;

    public LightItemViewHolder(View view) {
        icon = view.findViewById(R.id.light_icon);
        checkBox = (CheckBox) view.findViewById(R.id.light_checkbox);
        switchButton = (Switch) view.findViewById(R.id.light_switch);
        textView1 = (TextView) view.findViewById(android.R.id.text1);
        textView2 = (TextView) view.findViewById(android.R.id.text2);
    }
}
