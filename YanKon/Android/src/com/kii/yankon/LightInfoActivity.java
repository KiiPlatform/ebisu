package com.kii.yankon;

import android.app.Activity;
import android.database.Cursor;
import android.graphics.Color;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import com.kii.yankon.model.Action;
import com.kii.yankon.model.YanKonColor;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.widget.ColorCircle;
import com.kii.yankon.widget.ColorSlider;
import com.kii.yankon.widget.OnColorChangedListener;

import java.util.ArrayList;
import java.util.List;


public class LightInfoActivity extends Activity implements OnColorChangedListener {

    public static final String EXTRA_LIGHT_ID = "light_id";
    public static final String EXTRA_GROUP_ID = "group_id";
    public static final String EXTRA_NAME = "name";


    int light_id = -1;
    int group_id = -1;
    String mName;

    ColorCircle mColorCircle;

    ColorSlider mSaturation;

    ColorSlider mValue;

    Spinner mColorSpinner;
    List<Action> mActions;
    List<YanKonColor> mColors;

    private boolean color_changed = false;
    private int color;

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
        loadColors();
        mColorSpinner = (Spinner) findViewById(R.id.color_spinner);

        ArrayAdapter<YanKonColor> colorAdapter = new ArrayAdapter<YanKonColor>(this, android.R.layout.simple_spinner_dropdown_item, mColors);
        mColorSpinner.setAdapter(colorAdapter);
    }

    void loadColors() {
        mColors = new ArrayList<YanKonColor>();
        YanKonColor dummyColor = new YanKonColor();
        dummyColor.name = getString(R.string.color_spinner_custom);
        mColors.add(dummyColor);
        Cursor c = getContentResolver().query(YanKonProvider.URI_COLORS, null, null, null, "created_time asc");
        while (c.moveToNext()) {
            YanKonColor color = new YanKonColor();
            color.name = c.getString(c.getColumnIndex("name"));
            mColors.add(color);
        }
        c.close();
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
}
