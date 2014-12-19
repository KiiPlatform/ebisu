package com.kii.yankon;

import android.app.Activity;
import android.content.ContentValues;
import android.database.Cursor;
import android.graphics.Color;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.Toast;

import com.kii.yankon.model.Action;
import com.kii.yankon.model.YanKonColor;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.Utils;
import com.kii.yankon.widget.ColorCircle;
import com.kii.yankon.widget.ColorSlider;
import com.kii.yankon.widget.OnColorChangedListener;

import java.util.ArrayList;
import java.util.List;


public class LightInfoActivity extends Activity implements OnColorChangedListener, AdapterView.OnItemSelectedListener, CompoundButton.OnCheckedChangeListener, SeekBar.OnSeekBarChangeListener {

    public static final String EXTRA_LIGHT_ID = "light_id";
    public static final String EXTRA_GROUP_ID = "group_id";
    public static final String EXTRA_NAME = "name";


    int light_id = -1;
    int group_id = -1;
    String mName;

    ColorCircle mColorCircle;

    ColorSlider mSaturation;

    ColorSlider mValue;

    Switch mSwitch;
    SeekBar mBrightnessSeekBar;
    SeekBar mCTSeekBar;

    Spinner mColorSpinner;
    List<Action> mActions;
    List<YanKonColor> mColors;

    private boolean color_changed = false;
    private int color;
    private int brightness;
    private int CT;
    private boolean is_on;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_light_info);
        light_id = getIntent().getIntExtra(EXTRA_LIGHT_ID, -1);
        group_id = getIntent().getIntExtra(EXTRA_GROUP_ID, -1);
        mName = getIntent().getStringExtra(EXTRA_NAME);
        if (!TextUtils.isEmpty(mName)) {
            setTitle(mName);
        }
        Cursor c = null;
        if (light_id >= 0) {
            c = getContentResolver().query(YanKonProvider.URI_LIGHTS, null, "_id=" + light_id, null, null);
        } else if (group_id >= 0) {
            c = getContentResolver().query(YanKonProvider.URI_LIGHT_GROUPS, null, "_id=" + group_id, null, null);
        }
        if (c == null || !c.moveToFirst()) {
            if (c != null) {
                c.close();
            }
            Toast.makeText(this, "Cannot load light/group info", Toast.LENGTH_SHORT).show();
            return;
        }
        color = c.getInt(c.getColumnIndex("color"));
        brightness = c.getInt(c.getColumnIndex("brightness"));
        CT = c.getInt(c.getColumnIndex("CT"));
        if (light_id >= 0) {
            is_on = c.getInt(c.getColumnIndex("is_on")) != 0;
        } else {
            is_on = c.getInt(c.getColumnIndex("num")) == c.getInt(c.getColumnIndex("on_num"));
            saveChange(null);
        }
        c.close();
        initializeColor(color);
        loadColors();
        mColorSpinner = (Spinner) findViewById(R.id.color_spinner);
        ArrayAdapter<YanKonColor> colorAdapter = new ArrayAdapter<YanKonColor>(this, android.R.layout.simple_spinner_dropdown_item, mColors);
        mColorSpinner.setAdapter(colorAdapter);
        mColorSpinner.setOnItemSelectedListener(this);
        matchColor();

        mSwitch = (Switch) findViewById(R.id.light_switch);
        mSwitch.setChecked(is_on);
        mSwitch.setOnCheckedChangeListener(this);
        mBrightnessSeekBar = (SeekBar) findViewById(R.id.seekbar_brightness);
        mBrightnessSeekBar.setProgress(brightness);
        mBrightnessSeekBar.setOnSeekBarChangeListener(this);
        mCTSeekBar = (SeekBar) findViewById(R.id.seekbar_ct);
        mCTSeekBar.setProgress(CT);
        mCTSeekBar.setOnSeekBarChangeListener(this);
    }

    void loadColors() {
        mColors = new ArrayList<YanKonColor>();
        YanKonColor dummyColor = new YanKonColor();
        dummyColor.name = getString(R.string.color_spinner_custom);
        dummyColor.value = -1;
        mColors.add(dummyColor);
        Cursor c = getContentResolver().query(YanKonProvider.URI_COLORS, null, null, null, "created_time asc");
        while (c.moveToNext()) {
            YanKonColor color = new YanKonColor();
            color.name = c.getString(c.getColumnIndex("name"));
            color.value = c.getInt(c.getColumnIndex("value"));
            mColors.add(color);
        }
        c.close();
    }

    void matchColor() {
        for (int i = 0; i < mColors.size(); i++) {
            YanKonColor c = mColors.get(i);
            if (c.value == color) {
                mColorSpinner.setSelection(i);
                return;
            }
        }
        mColorSpinner.setSelection(0);
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

    void saveChange(ContentValues values) {
        if (values == null) {
            if (group_id >= 0) {
                Utils.controlGroup(this, group_id, is_on);
            }
            return;
        }
        values.put("synced", false);
        if (light_id >= 0) {
            getContentResolver().update(YanKonProvider.URI_LIGHTS, values, "_id=" + light_id, null);
            Utils.controlLight(this, light_id);
        } else {
            getContentResolver().update(YanKonProvider.URI_LIGHT_GROUPS, values, "_id=" + group_id, null);
            Utils.controlGroup(this, group_id, is_on);
        }
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
        matchColor();
        ContentValues values = new ContentValues();
        values.put("color", color);
        saveChange(values);
    }

    @Override
    public void onColorPicked(View view, int newColor) {

    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_light_info, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        switch (id) {
            case android.R.id.home:
                finish();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (parent == mColorSpinner) {
            if (position > 0) {
                YanKonColor c = mColors.get(position);
                initializeColor(c.value);
            }
        }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {

    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        is_on = isChecked;
        ContentValues values = null;
        if (light_id >= 0) {
            values = new ContentValues();
            values.put("is_on", is_on);
        }
        saveChange(values);
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (!fromUser) {
            return;
        }
        if (seekBar == mBrightnessSeekBar) {
            brightness = progress;
            ContentValues values = new ContentValues();
            values.put("brightness", brightness);
            saveChange(values);
        } else if (seekBar == mCTSeekBar) {
            CT = progress;
            ContentValues values = new ContentValues();
            values.put("CT", CT);
            saveChange(values);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }
}
