package com.kii.yankon.activities;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import com.kii.yankon.R;

public class RepeatDaysActivity extends ListActivity {

    boolean repeatDays[];

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActionBar().setHomeButtonEnabled(true);
        getActionBar().setDisplayHomeAsUpEnabled(true);
        repeatDays = getIntent().getBooleanArrayExtra("days");
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_multiple_choice);
        String[] dayNames = getResources().getStringArray(R.array.days);
        adapter.addAll(dayNames);
        setListAdapter(adapter);
        getListView().setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
        if (repeatDays != null) {
            for (int i = 0; i < 7 && i < repeatDays.length; i++) {
                getListView().setItemChecked(i, repeatDays[i]);
            }
        }

    }


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.repeat_days, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();

        switch (id) {
            case android.R.id.home:
                finish();
                return true;
            case R.id.action_done: {
                if (repeatDays == null) {
                    repeatDays = new boolean[7];
                }
                for (int i = 0; i < 7; i++) {
                    repeatDays[i] = getListView().isItemChecked(i);
                }
                Intent intent = new Intent();
                intent.putExtra("days", repeatDays);
                setResult(RESULT_OK, intent);
                finish();
            }
            return true;
        }

        return super.onOptionsItemSelected(item);
    }
}
