package com.kii.yankon;

import com.kii.yankon.widget.ColorCircle;
import com.kii.yankon.widget.ColorSlider;
import com.kii.yankon.widget.OnColorChangedListener;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.View;


public class ColorPickerActivity2 extends Activity implements OnColorChangedListener {

    /**
     * Color.
     *
     * <p>Color as integer value, as used in setColor() and related.</p>
     *
     * <p>Constant Value: "org.openintents.extra.COLOR"</p>
     */
    public final static String EXTRA_COLOR = "com.kii.extra.COLOR";

    public final static String EXTRA_NAME = "com.kii.extra.NAME";

    public final static String EXTRA_ID = "com.kii.extra.ID";

    ColorCircle mColorCircle;

    ColorSlider mSaturation;

    ColorSlider mValue;

    Intent mIntent;

    private boolean color_changed = false;

    private static final String TAG = ColorPickerActivity2.class.getSimpleName();

    private int id;

    private String name;

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean("color_changed", this.color_changed);
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        this.color_changed = savedInstanceState.getBoolean("color_changed");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.colorpicker);

        // Get original color
        mIntent = getIntent();
        if (mIntent == null) {
            mIntent = new Intent();
        }

        int color = mIntent.getIntExtra(EXTRA_COLOR, Color.BLACK);
        id = mIntent.getIntExtra(EXTRA_ID, -1);
        name = mIntent.getStringExtra(EXTRA_NAME);
        Log.d(TAG, String.format("Input color is %x", color));
        initializeColor(color);
    }

    void initializeColor(int color) {

        mColorCircle = (ColorCircle) findViewById(R.id.colorcircle);
        mColorCircle.setOnColorChangedListener(this);
        mColorCircle.setColor(color);

        mSaturation = (ColorSlider) findViewById(R.id.saturation);
        mSaturation.setOnColorChangedListener(this);
        mSaturation.setColors(color, Color.BLACK);

        mValue = (ColorSlider) findViewById(R.id.value);
        mValue.setOnColorChangedListener(this);
        mValue.setColors(Color.WHITE, color);
    }


    class ColorPickerState {

        int mColor;
    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        ColorPickerState state = new ColorPickerState();
        state.mColor = this.mColorCircle.getColor();
        return state;
    }


    public int toGray(int color) {
        int a = Color.alpha(color);
        int r = Color.red(color);
        int g = Color.green(color);
        int b = Color.blue(color);
        int gray = (r + g + b) / 3;
        return Color.argb(a, gray, gray, gray);
    }


    public void onColorChanged(View view, int newColor) {
        if (view == mColorCircle) {
            mValue.setColors(0xFFFFFFFF, newColor);
            mSaturation.setColors(newColor, 0xff000000);
        } else if (view == mSaturation) {
            mColorCircle.setColor(newColor);
            mValue.setColors(0xFFFFFFFF, newColor);
        } else if (view == mValue) {
            mColorCircle.setColor(newColor);
        }

        this.color_changed = true;
    }


    public void onColorPicked(View view, int newColor) {
        // We can return result
        Log.d(TAG, String.format("newColor is %x", newColor));
        mIntent.putExtra(EXTRA_COLOR, newColor);
        mIntent.putExtra(EXTRA_ID, id);
        mIntent.putExtra(EXTRA_NAME, name);
        setResult(RESULT_OK, mIntent);
        finish();
    }

}