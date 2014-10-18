
package com.youwill.store;

import com.youwill.store.fragments.CategoriesFragment;
import com.youwill.store.fragments.HotFragment;
import com.youwill.store.fragments.PurchasedFragment;
import com.youwill.store.fragments.UpgradeFragment;
import com.youwill.store.utils.DataUtils;

import android.app.Activity;
import android.app.Fragment;
import android.os.Bundle;
import android.view.View;

import java.util.HashMap;
import java.util.Map;

public class MainActivity extends Activity implements View.OnClickListener {

    private static final String TAG = MainActivity.class.getSimpleName();

    private String[] tabs = new String[4];

    private int currentFragmentIndex;

    private Fragment currentFragment;

    private Map<Integer, Fragment> fragments = new HashMap<Integer, Fragment>(4);

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

}
