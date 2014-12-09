package com.kii.yankon;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.EditText;
import android.widget.ListView;

import java.util.HashSet;

/**
 * Created by Evan on 14/12/8.
 */
public class AddScenesActivity extends Activity implements View.OnClickListener {

    public static final String EXTRA_SCENE_ID = "scene_id";
    public static final String EXTRA_SCENE_NAME = "scene_name";

    EditText mSceneNameEdit;
    ListView mList;
    int scene_id;
    HashSet<String> orgSelectedSet = new HashSet<String>();
    HashSet<String> selectedSet = new HashSet<String>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_light_groups);
        mSceneNameEdit = (EditText) findViewById(R.id.scene_name);
        mList = (ListView) findViewById(R.id.scene_list);
        findViewById(R.id.scene_cancel).setOnClickListener(this);
        findViewById(R.id.scene_ok).setOnClickListener(this);

        scene_id = getIntent().getIntExtra(EXTRA_SCENE_ID, -1);
        mSceneNameEdit.setText(getIntent().getStringExtra(EXTRA_SCENE_NAME));
    }

    @Override
    public void onClick(View v) {

    }
}
