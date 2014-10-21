package com.youwill.store;

import com.youwill.store.fragments.CategoriesFragment;
import com.youwill.store.fragments.HotFragment;
import com.youwill.store.fragments.PurchasedFragment;
import com.youwill.store.fragments.UpgradeFragment;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.DataUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.Fragment;
import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AutoCompleteTextView;
import android.widget.CursorAdapter;
import android.widget.TextView;

import java.util.HashMap;
import java.util.Map;

public class MainActivity extends Activity implements View.OnClickListener,
        AdapterView.OnItemClickListener {

    private static final String TAG = MainActivity.class.getSimpleName();

    private String[] tabs = new String[4];

    private int currentFragmentIndex;

    private Fragment currentFragment;

    private Map<Integer, Fragment> fragments = new HashMap<Integer, Fragment>(4);

    private AutoCompleteTextView searchEdit;

    private View searchButton;

    private View deleteButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        tabs[0] = getString(R.string.hot_sale);
        tabs[1] = getString(R.string.categories);
        tabs[2] = getString(R.string.purchased);
        tabs[3] = getString(R.string.upgrade);
        findViewById(R.id.hot_button).setOnClickListener(this);
        findViewById(R.id.categories_button).setOnClickListener(this);
        findViewById(R.id.upgrade_button).setOnClickListener(this);
        findViewById(R.id.purchased_button).setOnClickListener(this);
        if (savedInstanceState == null) {
            fragments.put(R.id.hot_button,
                    Fragment.instantiate(this, HotFragment.class.getName()));
            fragments.put(R.id.categories_button,
                    Fragment.instantiate(this, CategoriesFragment.class.getName()));
            fragments.put(R.id.upgrade_button,
                    Fragment.instantiate(this, UpgradeFragment.class.getName()));
            fragments.put(R.id.purchased_button,
                    Fragment.instantiate(this, PurchasedFragment.class.getName()));
            currentFragment = fragments.get(R.id.hot_button);
            currentFragmentIndex = R.id.hot_button;
            getFragmentManager().beginTransaction()
                    .add(R.id.fragments, currentFragment, String.valueOf(currentFragmentIndex))
                    .commit();
        }
        DataUtils.loadApps(this);
        initHeader();
    }

    private void initHeader() {
        View header = findViewById(R.id.main_header);
        searchEdit = (AutoCompleteTextView) header.findViewById(R.id.search_edit);
        searchEdit.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                startQuery();
            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });
        searchButton = header.findViewById(R.id.search_button);
        deleteButton = header.findViewById(R.id.delete_button);
        searchButton.setOnClickListener(this);
        deleteButton.setOnClickListener(this);
        mAdapter = new SearchAdapter();
        startQuery();
        searchEdit.setAdapter(mAdapter);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.categories_button:
                //fall through;
            case R.id.hot_button:
                //fall through;
            case R.id.purchased_button:
                //fall through;
            case R.id.upgrade_button:
                switchFragment(v);
                break;
            case R.id.search_button:
                //TODO
                break;
            case R.id.delete_button:
                searchEdit.setText("");
                break;
        }

    }

    private void switchFragment(View v) {
        if (currentFragmentIndex == v.getId()) {
            return;
        }

        Fragment fragment = fragments.get(v.getId());
        if (fragment.isAdded()) {
            getFragmentManager().beginTransaction().hide(currentFragment).show(fragment).commit();
        } else {
            getFragmentManager().beginTransaction().hide(currentFragment)
                    .add(R.id.fragments, fragment, String.valueOf(v.getId())).commit();
        }
        currentFragmentIndex = v.getId();
        currentFragment = fragment;
        DataUtils.loadApps(this);
    }

    private Cursor mCursor;

    private SearchAdapter mAdapter;

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mCursor.moveToPosition(position);
        //TODO: launch app detail activity;
    }

    private void startQuery() {
        if (TextUtils.isEmpty(searchEdit.getText().toString())) {
            mCursor = null;
            mAdapter.changeCursor(mCursor);
        } else {
            mCursor = getContentResolver().query(YouWill.Application.CONTENT_URI, COLUMNS,
                    YouWill.Application.SEARCH_FIELD + " LIKE %" + searchEdit.getText() + "%", null,
                    null);
            mAdapter.changeCursor(mCursor);
        }
        mAdapter.notifyDataSetChanged();
    }

    private class SearchAdapter extends CursorAdapter {

        public SearchAdapter() {
            super(MainActivity.this, mCursor, true);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return null;
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {

        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            mCursor.moveToPosition(position);
            View v = convertView;
            if (v == null) {
                v = getLayoutInflater().inflate(R.layout.search_list_item, parent, false);
            }
            TextView nameView = (TextView) v.findViewById(R.id.name);
            try {
                JSONObject app = new JSONObject(mCursor.getString(2));
                String appName = app.optString("name");
                nameView.setText(appName);
            } catch (JSONException e) {
                e.printStackTrace();
            }
            return v;
        }
    }

    private static final String[] COLUMNS = new String[]{
            YouWill.Application.APP_ID,         //0
            YouWill.Application.APP_PACKAGE,    //1
            YouWill.Application.APP_INFO,       //2
    };


}
