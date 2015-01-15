package com.kii.yankon.activities;

import com.kii.yankon.R;
import com.kii.yankon.widget.ColorCircle;
import com.kii.yankon.widget.ColorSlider;
import com.kii.yankon.widget.OnColorChangedListener;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;


public class ColorPickerActivity2 extends Activity implements OnColorChangedListener {

    /**
     * Color.
     * <p/>
     * <p>Color as integer value, as used in setColor() and related.</p>
     * <p/>
     * <p>Constant Value: "org.openintents.extra.COLOR"</p>
     */
    public static final String EXTRA_COLOR = "color";
    public static final String EXTRA_NAME = "name";
    public static final String EXTRA_ID = "id";

    ColorCircle mColorCircle;

    ColorSlider mSaturation;

    ColorSlider mValue;

    EditText mEdit;

    Intent mIntent;

    private boolean color_changed = false;

    private static final String TAG = ColorPickerActivity2.class.getSimpleName();

    private int id;

    private String name;

    private int color;

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

        color = mIntent.getIntExtra(EXTRA_COLOR, Color.BLACK);
        id = mIntent.getIntExtra(EXTRA_ID, -1);
        name = mIntent.getStringExtra(EXTRA_NAME);
        Log.d(TAG, String.format("Input color is %x", color));
        initializeColor(color);

        mEdit = (EditText) findViewById(R.id.edit);
        mEdit.setVisibility(name == null ? View.GONE : View.VISIBLE);
        mEdit.setText(name);
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
        color = newColor;
        this.color_changed = true;
    }


    public void onColorPicked(View view, int newColor) {
        color = newColor;
        Log.d(TAG, String.format("newColor is %x", newColor));
    }

    public void save() {
        name = mEdit.getText().toString();
        mIntent.putExtra(EXTRA_COLOR, color);
        mIntent.putExtra(EXTRA_ID, id);
        mIntent.putExtra(EXTRA_NAME, name);
        setResult(RESULT_OK, mIntent);
        finish();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.done, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_done:
                save();
                break;
            case android.R.id.home:
                finish();
                break;

        }
        return true;
    }

}