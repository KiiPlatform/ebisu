package com.kii.yankon;

import com.larswerkman.holocolorpicker.ColorPicker;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import butterknife.ButterKnife;
import butterknife.InjectView;


public class ColorPickerActivity extends Activity implements ColorPicker.OnColorChangedListener, View.OnClickListener {

    private static final String TAG = ColorPickerActivity.class.getSimpleName();

    @InjectView(R.id.picker)
    ColorPicker mPicker;

    @InjectView(R.id.confirm_button)
    View mConfirmButton;

    private int mColorValue;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_color_picker);
        ButterKnife.inject(this);
        mPicker.setOnColorChangedListener(this);
        mConfirmButton.setOnClickListener(this);
    }

    @Override
    public void onColorChanged(int i) {
        mColorValue = i;
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.confirm_button:
                Intent intent = new Intent();
                intent.putExtra("color", mColorValue);
                setResult(RESULT_OK, intent);
                finish();
                break;
            default:
                break;
        }
    }
}
