package com.kii.yankon;

import android.app.Activity;
import android.database.Cursor;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import com.kii.yankon.model.Action;
import com.kii.yankon.model.YanKonColor;
import com.kii.yankon.providers.YanKonProvider;

import java.util.ArrayList;
import java.util.List;


public class LightInfoActivity extends Activity {

    public static final String EXTRA_LIGHT_ID = "light_id";
    public static final String EXTRA_GROUP_ID = "group_id";
    public static final String EXTRA_NAME = "name";


    int light_id = -1;
    int group_id = -1;
    String mName;

    Spinner mActionSpinner;
    Spinner mColorSpinner;
    List<Action> mActions;
    List<YanKonColor> mColors;

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
        loadActionsAndColors();
        mActionSpinner = (Spinner) findViewById(R.id.action_spinner);
        mColorSpinner = (Spinner) findViewById(R.id.color_spinner);
        ArrayAdapter<Action> actionAdapter = new ArrayAdapter<Action>(this, android.R.layout.simple_spinner_dropdown_item, mActions);
        mActionSpinner.setAdapter(actionAdapter);
        ArrayAdapter<YanKonColor> colorAdapter = new ArrayAdapter<YanKonColor>(this, android.R.layout.simple_spinner_dropdown_item, mColors);
        mColorSpinner.setAdapter(colorAdapter);
    }

    void loadActionsAndColors() {
        mActions = new ArrayList<Action>();
        Action dummyAction = new Action();
        dummyAction.name = getString(R.string.action_spinner_default);
        mActions.add(dummyAction);
        Cursor c = getContentResolver().query(YanKonProvider.URI_ACTIONS, null, null, null, "created_time asc");
        while (c.moveToNext()) {
            Action action = new Action();
            action.name = c.getString(c.getColumnIndex("name"));
            mActions.add(action);
        }
        c.close();
        mColors = new ArrayList<YanKonColor>();
        YanKonColor dummyColor = new YanKonColor();
        dummyColor.name = getString(R.string.color_spinner_custom);
        mColors.add(dummyColor);
        c = getContentResolver().query(YanKonProvider.URI_COLORS, null, null, null, "created_time asc");
        while (c.moveToNext()) {
            YanKonColor color = new YanKonColor();
            color.name = c.getString(c.getColumnIndex("name"));
            mColors.add(color);
        }
        c.close();
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
