package com.kii.yankon.activities;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TimePicker;
import android.widget.Toast;

import com.kii.yankon.R;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.Constants;


public class AddScheduleActivity extends Activity implements View.OnClickListener {

    public static final String EXTRA_SCHEDULE_ID = "schedule_id";

    private static final int REQUEST_SETTINGS = 0x1001;
    private static final int REQUEST_TARGET = 0x1002;
    EditText mNameField;
    int schedule_id = -1;
    int color = Constants.DEFAULT_COLOR;
    int brightness = Constants.DEFAULT_BRIGHTNESS;
    int CT = Constants.DEFAULT_CT;
    int state = 0;
    int time = 0;
    Button mTargetBtn, mSettingsBtn;
    TimePicker mTimePicker;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_schedule);
        mTargetBtn = (Button) findViewById(R.id.target_btn);
        mTargetBtn.setOnClickListener(this);
        mSettingsBtn = (Button) findViewById(R.id.settings_btn);
        mSettingsBtn.setOnClickListener(this);
        findViewById(R.id.schedule_cancel).setOnClickListener(this);
        findViewById(R.id.schedule_ok).setOnClickListener(this);
        mNameField = (EditText) findViewById(R.id.schedule_name);
        mTimePicker = (TimePicker) findViewById(R.id.time_picker);

        schedule_id = getIntent().getIntExtra(EXTRA_SCHEDULE_ID, -1);
        if (schedule_id >= 0) {
            Cursor c = getContentResolver().query(YanKonProvider.URI_SCHEDULE, null, "_id=" + schedule_id, null, null);
            if (c != null) {
                if (c.moveToFirst()) {
                    String name = c.getString(c.getColumnIndex("name"));
                    mNameField.setText(name);
                    color = c.getInt(c.getColumnIndex("color"));
                    brightness = c.getInt(c.getColumnIndex("brightness"));
                    CT = c.getInt(c.getColumnIndex("CT"));
                    state = c.getInt(c.getColumnIndex("state"));
                    time = c.getInt(c.getColumnIndex("time"));
                }
                c.close();
            }
        }
        updateSettingsInfo();
        mTimePicker.setCurrentHour(time / 60);
        mTimePicker.setCurrentMinute(time % 60);
    }

    public void updateSettingsInfo() {
        mSettingsBtn.setTextColor(color);
        mSettingsBtn.setText(getString(R.string.schedule_settings_format,
                state == 0 ? getString(R.string.state_off) : getString(R.string.state_on), brightness, CT));
    }


    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                return true;
            default:
                break;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.target_btn: {
                Intent intent = new Intent(this, PickTargetActivity.class);
                startActivityForResult(intent, REQUEST_TARGET);
            }
            break;
            case R.id.settings_btn: {
                Intent intent = new Intent(this, LightInfoActivity.class);
                intent.putExtra("state", state > 0);
                intent.putExtra("color", color);
                intent.putExtra("brightness", brightness);
                intent.putExtra("CT", CT);
                startActivityForResult(intent, REQUEST_SETTINGS);
            }
            break;
            case R.id.schedule_cancel:
                finish();
                break;
            case R.id.schedule_ok:
                save();
                break;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (resultCode == RESULT_OK) {
            if (requestCode == REQUEST_SETTINGS) {
                state = data.getBooleanExtra("state", true) ? 1 : 0;
                color = data.getIntExtra("color", Constants.DEFAULT_COLOR);
                brightness = data.getIntExtra("brightness", Constants.DEFAULT_BRIGHTNESS);
                CT = data.getIntExtra("CT", Constants.DEFAULT_CT);
                updateSettingsInfo();
            }
        }
    }

    private void save() {
        String gName = mNameField.getText().toString();
        if (TextUtils.isEmpty(gName)) {
            Toast.makeText(this, getString(R.string.empty_scene_name), Toast.LENGTH_SHORT).show();
            return;
        }
        ContentResolver cr = getContentResolver();
        ContentValues values = new ContentValues();
        values.put("name", gName);
        values.put("color", color);
        values.put("brightness", brightness);
        values.put("CT", CT);
        values.put("state", state);
        values.put("enabled", true);
        values.put("time", mTimePicker.getCurrentHour() * 60 + mTimePicker.getCurrentMinute());
        if (schedule_id < 0) {
            values.put("created_time", System.currentTimeMillis());
            Uri uri = cr.insert(YanKonProvider.URI_SCHEDULE, values);
            schedule_id = Integer.parseInt(uri.getLastPathSegment());
        } else {
            cr.update(YanKonProvider.URI_SCHEDULE, values, "_id=" + schedule_id, null);
        }

        finish();
    }

}
