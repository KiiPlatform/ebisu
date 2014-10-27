package com.youwill.store;

import com.youwill.store.fragments.CategoriesFragment;
import com.youwill.store.fragments.HotFragment;
import com.youwill.store.fragments.PurchasedFragment;
import com.youwill.store.fragments.SearchFragment;
import com.youwill.store.fragments.UpgradeFragment;
import com.youwill.store.utils.DataUtils;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.EditText;

import java.util.HashMap;
import java.util.Map;

public class MainActivity extends Activity implements View.OnClickListener {

    private static final String TAG = MainActivity.class.getSimpleName();

    private String[] tabs = new String[4];

    private int currentFragmentIndex;

    private Fragment currentFragment;

    private Map<Integer, Fragment> fragments = new HashMap<Integer, Fragment>(4);

    private EditText searchEdit;

    private View searchButton;

    private View deleteButton;

    private SearchFragment mSearchFragment;

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
        mSearchFragment = (SearchFragment) Fragment
                .instantiate(this, SearchFragment.class.getName());
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
        searchEdit = (EditText) header.findViewById(R.id.search_edit);
        searchButton = header.findViewById(R.id.search_button);
        deleteButton = header.findViewById(R.id.delete_button);
        searchButton.setOnClickListener(this);
        deleteButton.setOnClickListener(this);
        deleteButton.requestFocus();
        searchEdit.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (hasFocus && !mSearchFragment.isAdded()) {
                    showSearchFragment();
                }
            }
        });
        searchEdit.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (s != null && mSearchFragment.isAdded()) {
                    mSearchFragment.beginSearch(s.toString());
                }
            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });
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
            case R.id.delete_button:
                searchEdit.setText("");
                break;
        }

    }

    private void showSearchFragment() {
        if (mSearchFragment.isAdded()) {
            mSearchFragment.beginSearch(searchEdit.getText().toString());
        } else {
            FragmentTransaction transaction = getFragmentManager().beginTransaction();
            transaction.add(R.id.fragments, mSearchFragment);
            transaction.addToBackStack(null);
            transaction.commit();
            searchEdit.post(new Runnable() {
                @Override
                public void run() {
                    mSearchFragment.beginSearch(searchEdit.getText().toString());
                }
            });
        }
    }

    @Override
    public void onBackPressed() {
        if (mSearchFragment.isAdded()) {
            getFragmentManager().popBackStack();
        } else {
            finish();
        }
    }

    private void switchFragment(View v) {
        if (mSearchFragment.isAdded()) {
            getFragmentManager().popBackStack();
        }
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

}
