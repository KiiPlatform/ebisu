package com.youwill.store.sandbox;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.EditText;

public class MainActivity extends Activity implements View.OnClickListener {

    private EditText packageNameEdit;

    private EditText keyEdit;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        findViewById(R.id.confirm).setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.confirm:
                Settings.saveAppKey(this, packageNameEdit.getText().toString(),
                        keyEdit.getText().toString());
                break;
        }
    }
}
