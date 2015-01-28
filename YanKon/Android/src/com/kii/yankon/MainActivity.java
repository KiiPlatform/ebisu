package com.kii.yankon;

import android.app.ActionBar;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Fragment;
import android.app.FragmentManager;
import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.PreferenceManager;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v4.widget.DrawerLayout;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.LoginCallBack;
import com.kii.yankon.activities.GuideActivity;
import com.kii.yankon.fragments.ActionsFragment;
import com.kii.yankon.fragments.ColorsFragment;
import com.kii.yankon.fragments.LightGroupsFragment;
import com.kii.yankon.fragments.LightsFragment;
import com.kii.yankon.fragments.LogInFragment;
import com.kii.yankon.fragments.PlaceholderFragment;
import com.kii.yankon.fragments.ProfileFragment;
import com.kii.yankon.fragments.ScenesFragment;
import com.kii.yankon.fragments.ScheduleFragment;
import com.kii.yankon.fragments.SettingsFragment;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.services.NetworkReceiverService;
import com.kii.yankon.services.NetworkSenderService;
import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.Global;
import com.kii.yankon.utils.Network;
import com.kii.yankon.utils.Settings;

import java.lang.ref.WeakReference;

public class MainActivity extends Activity
        implements NavigationDrawerFragment.NavigationDrawerCallbacks, LoginCallBack {

    /**
     * Fragment managing the behaviors, interactions and presentation of the
     * navigation drawer.
     */
    public NavigationDrawerFragment mNavigationDrawerFragment;

    /**
     * Used to store the last screen title. For use in
     * {@link #restoreActionBar()}.
     */
    private CharSequence mTitle;

    MainHandler mHandler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
        mNavigationDrawerFragment = (NavigationDrawerFragment)
                getFragmentManager().findFragmentById(R.id.navigation_drawer);
        mTitle = getTitle();

        // Set up the drawer.
        mNavigationDrawerFragment.setUp(
                R.id.navigation_drawer,
                (DrawerLayout) findViewById(R.id.drawer_layout));
        setDefaultHomePage();

        if (Settings.needShowGuide()) {
            startActivity(new Intent(this, GuideActivity.class));
            finish();
            return;
        }
        IntentFilter filter = new IntentFilter(Constants.INTENT_LOGGED_IN);
        filter.addAction(Constants.INTENT_LOGGED_OUT);
        LocalBroadcastManager.getInstance(this).registerReceiver(mReceiver, filter);
        startService(new Intent(this, NetworkReceiverService.class));

        loginKii();
        mHandler = new MainHandler(this);
    }

    private void setDefaultHomePage() {
        int index = 0;
        try {
            index = Integer.parseInt(PreferenceManager.getDefaultSharedPreferences(this)
                    .getString("default_home_page", "0"));
        } catch (Exception ignored) {

        }
        mNavigationDrawerFragment.selectItem(index);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mHandler.sendEmptyMessage(MainHandler.MSG_UPDATE_STATUS);
    }

    @Override
    protected void onPause() {
        super.onPause();
        mHandler.sendEmptyMessage(MainHandler.MSG_STOP);
    }

    @Override
    protected void onDestroy() {
        LocalBroadcastManager.getInstance(this).unregisterReceiver(mReceiver);
        stopService(new Intent(this, NetworkReceiverService.class));
        if (mHandler != null) {
            mHandler.sendEmptyMessage(MainHandler.MSG_STOP);
            mHandler = null;
        }
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        AlertDialog.Builder ab = new AlertDialog.Builder(this);
        ab.setMessage(R.string.exit_prompt);
        ab.setNegativeButton(android.R.string.cancel, null);
        ab.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                finish();
            }
        });
        ab.show();
    }

    static class MainHandler extends Handler {
        public static final int MSG_UPDATE_STATUS = 0;
        public static final int MSG_STOP = 1;
        WeakReference<MainActivity> actRef = null;

        MainHandler(MainActivity activity) {
            super();
            actRef = new WeakReference<>(activity);
        }

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            if (actRef == null) {
                return;
            }
            MainActivity activity = actRef.get();
            if (activity == null) {
                return;
            }
            switch (msg.what) {
                case MSG_STOP:
                    removeMessages(MSG_UPDATE_STATUS);
                    break;
                case MSG_UPDATE_STATUS: {
                    removeMessages(MSG_UPDATE_STATUS);
                    Network.getLocalIP(activity);
                    ContentValues values = new ContentValues();
                    values.put("connected", false);
                    values.put("IP", "");
                    activity.getContentResolver().update(YanKonProvider.URI_LIGHTS, values, null, null);

                    if (Global.isWifiConnected) {
                        NetworkSenderService.sendCmd(activity, (String) null, Constants.SEARCH_LIGHTS_CMD);
                    }
                    sendEmptyMessageDelayed(MSG_UPDATE_STATUS, 20 * 1000);
                }
                break;
            }
        }
    }

    @Override
    public void onNavigationDrawerItemSelected(int position) {
        // update the main content by replacing fragments
        FragmentManager fragmentManager = getFragmentManager();
        Fragment fragment;
        switch (position) {
            case 0:
                fragment = LightsFragment.newInstance(position + 1);
                break;
            case 1:
                fragment = LightGroupsFragment.newInstance(position + 1);
                break;
            case 2:
                fragment = ScenesFragment.newInstance(position + 1);
                break;
            case 3:
                fragment = ScheduleFragment.newInstance(position + 1);
                break;
            case 4:
                fragment = ActionsFragment.newInstance(position + 1);
                break;
            case 5:
                fragment = ColorsFragment.newInstance(position + 1);
                break;
            case 6:
                if (Settings.isLoggedIn()) {
                    fragment = ProfileFragment.newInstance(position + 1);
                } else {
                    fragment = LogInFragment.newInstance(position + 1);
                }
                break;
            case 7:
                fragment = SettingsFragment.newInstance(position + 1);
                break;
            default:
                fragment = PlaceholderFragment.newInstance(position + 1);
                break;
        }
        fragmentManager.beginTransaction()
                .replace(R.id.container, fragment)
                .commit();
    }

    public void onSectionAttached(int number) {
        switch (number) {
            case 1:
                mTitle = getString(R.string.title_section1);
                break;
            case 2:
                mTitle = getString(R.string.title_section2);
                break;
            case 3:
                mTitle = getString(R.string.title_section3);
                break;
            case 4:
                mTitle = getString(R.string.title_section4);
                break;
            case 5:
                mTitle = getString(R.string.title_section5);
                break;
            case 6:
                mTitle = getString(R.string.title_section6);
                break;
            case 7:
                mTitle = getString(R.string.title_section7);
                break;
            case 8:
                mTitle = getString(R.string.title_section8);
                break;
        }
    }

    public void restoreActionBar() {
        ActionBar actionBar = getActionBar();
        actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_STANDARD);
        actionBar.setDisplayShowTitleEnabled(true);
        actionBar.setTitle(mTitle);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (!mNavigationDrawerFragment.isDrawerOpen()) {
            // Only show items in the action bar relevant to this screen
            // if the drawer is not showing. Otherwise, let the drawer
            // decide what to show in the action bar.
//            getMenuInflater().inflate(R.menu.main, menu);
            restoreActionBar();
            return true;
        }
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return super.onOptionsItemSelected(item);
    }

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.d("MainActivity", "onReceive, action is " + action);
            if (action.equals(Constants.INTENT_LOGGED_IN) ||
                    action.equals(Constants.INTENT_LOGGED_OUT)) {
                onNavigationDrawerItemSelected(6);
            }
        }
    };

    protected void loginKii() {
        String token = Settings.getToken();
        if (!KiiUser.isLoggedIn() && !TextUtils.isEmpty(token)) {
            KiiUser.loginWithToken(this, token, Settings.getExp());
        }
    }

    @Override
    public void onLoginCompleted(KiiUser kiiUser, Exception e) {
        if (kiiUser != null) {
            Toast.makeText(this, "Kii logged in", Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(this, "Kii login failed", Toast.LENGTH_SHORT).show();
        }
    }


}
