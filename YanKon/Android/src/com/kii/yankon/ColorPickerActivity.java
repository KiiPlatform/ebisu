package com.kii.yankon;

import com.larswerkman.holocolorpicker.ColorPicker;
import com.larswerkman.holocolorpicker.OpacityBar;
import com.larswerkman.holocolorpicker.SVBar;
import com.larswerkman.holocolorpicker.SaturationBar;
import com.larswerkman.holocolorpicker.ValueBar;

import android.app.Activity;
import android.os.Bundle;

import butterknife.ButterKnife;
import butterknife.InjectView;


public class ColorPickerActivity extends Activity implements ColorPicker.OnColorChangedListener {

    @InjectView(R.id.picker)
    ColorPicker mPicker;

    @InjectView(R.id.svbar)
    SVBar mSVBar;

    @InjectView(R.id.opacitybar)
    OpacityBar mOpacityBar;

    @InjectView(R.id.saturationbar)
    SaturationBar mSaturationBar;

    @InjectView(R.id.valuebar)
    ValueBar mValueBar;

    private int mColorValue;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_color_picker);
        ButterKnife.inject(this);
        mPicker.addSVBar(mSVBar);
        mPicker.addOpacityBar(mOpacityBar);
        mPicker.addSaturationBar(mSaturationBar);
        mPicker.addValueBar(mValueBar);
        mPicker.setOldCenterColor(mPicker.getColor());
        mPicker.setOnColorChangedListener(this);
    }

    @Override
    public void onColorChanged(int i) {
        mColorValue = i;
    }
}
