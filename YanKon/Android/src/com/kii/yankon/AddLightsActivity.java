package com.kii.yankon;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v4.view.PagerAdapter;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import com.kii.yankon.fragments.InputDialogFragment;
import com.kii.yankon.model.Light;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.services.NetworkSenderService;
import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.Global;
import com.kii.yankon.utils.KiiSync;
import com.pixplicity.multiviewpager.MultiViewPager;

import java.util.ArrayList;
import java.util.List;


/**
 * Created by tian on 14/11/24:下午11:50.
 */
public class AddLightsActivity extends Activity implements View.OnClickListener, InputDialogFragment.InputDialogInterface {

    private static final String LOG_TAG = "AddLightsActivity";

    private MultiViewPager mPager;
    private List<Light> mLights = new ArrayList<Light>();
    private LightsAdapter mAdapter;
    private Light currLight = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_lights);


        mPager = (MultiViewPager) findViewById(R.id.pager);

        mAdapter = new LightsAdapter();
/*
        Light l = new Light();
        l.name = "Unknown1";
        l.model = "model1";
        l.mac = "11";
        Global.gLightsMacMap.put(l.mac, l);
        addLight(l);
        l = new Light();
        l.name = "Unknown2";
        l.model = "model1";
        l.mac = "22";
        Global.gLightsMacMap.put(l.mac, l);
        addLight(l);
        l = new Light();
        l.name = "Unknown3";
        l.model = "model1";
        l.mac = "33";
        Global.gLightsMacMap.put(l.mac, l);
        addLight(l);
        l = new Light();
        l.name = "Unknown4";
        l.model = "model1";
        l.mac = "44";
        Global.gLightsMacMap.put(l.mac, l);
        addLight(l);
        l = new Light();
        l.name = "Unknown5";
        l.model = "model1";
        l.mac = "55";
        Global.gLightsMacMap.put(l.mac, l);
        addLight(l);
*/

        mPager.setAdapter(mAdapter);

        IntentFilter filter = new IntentFilter(Constants.ACTION_LIGHT_UPDATED);
        LocalBroadcastManager.getInstance(this).registerReceiver(mReceiver, filter);
        NetworkSenderService.sendCmd(this, (String) null, Constants.SEARCH_LIGHTS_CMD);
        /*
        byte[] cmd = new byte[] {00, 00, 01, 00, 0x44, 00, 01, 01, 00, 06, 00, 0x78, (byte)0xb3, (byte)0xb9, 0x0f, (byte)0xfe, (byte)0xf1,
                0x0a, 0x01, 0x00, 0x03, 00, 0x7f, 0x7f, 0x32, 0x0a, 02, 00, 01, 00, 0x32, 0x0a, 0x03, 00, 01, 00, 0x32, 00, 03
                ,00, 0x20, 00, 0x59, 0x61, 0x6e, 0x6b, 0x6f, 0x6e, 0x6c, 0x65, 0x64, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
                00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00};
        NetworkSenderService.sendCmd(this, (String) null, cmd);*/
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        LocalBroadcastManager.getInstance(this).unregisterReceiver(mReceiver);
    }

    BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            for (Light light : Global.gLightsMacMap.values()) {
                addLight(light);
            }
        }
    };

    void addLight(Light light) {
        if (mLights.contains(light)) {
            return;
        }
        Cursor cursor = getContentResolver().query(YanKonProvider.URI_LIGHTS, null, "MAC=(?)", new String[]{light.mac}, null);
        if (cursor.moveToFirst()) {
            light.name = cursor.getString(cursor.getColumnIndex("name"));
            light.model = cursor.getString(cursor.getColumnIndex("model"));
            light.added = true;
        }
        cursor.close();
        mLights.add(light);
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.light_button:
                currLight = (Light) v.getTag();
                showInputNameDialog();
                break;
        }
    }

    void showInputNameDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        Fragment prev = getFragmentManager().findFragmentByTag("dialog");
        if (prev != null) {
            ft.remove(prev);
        }
        ft.addToBackStack(null);

        InputDialogFragment newFragment = InputDialogFragment.newInstance(
                getString(R.string.set_light_name),
                null,
                null
        );
        newFragment.show(ft, "dialog");
    }

    @Override
    public void onInputDialogTextDone(String text) {
        currLight.name = text;
        currLight.added = true;
        Log.e(LOG_TAG, "Mac:" + currLight.mac);
        ContentValues values = new ContentValues();
        values.put("MAC", currLight.mac);
        values.put("model", currLight.model);
        values.put("connected", true);
        values.put("state", currLight.state);
        values.put("IP", currLight.ip);
        values.put("color", currLight.color);
        values.put("brightness", Math.max(currLight.brightness, Constants.MIN_BRIGHTNESS));
        values.put("CT", Math.max(currLight.CT, Constants.MIN_CT));
        values.put("name", text);
        values.put("owned_time", System.currentTimeMillis());
        getContentResolver().insert(YanKonProvider.URI_LIGHTS, values);
        new Thread() {
            @Override
            public void run() {
                Cursor c = getContentResolver().query(YanKonProvider.URI_LIGHTS, null, "MAC=(?)", new String[]{currLight.mac}, null);
                KiiSync.syncLights(AddLightsActivity.this, c);
                c.close();
                String result = KiiSync.registLamp(currLight.mac);
                Log.e("Test", "Reg result:" + result);
            }
        }.start();
        View view = mPager.findViewWithTag(currLight);
        ViewHolder holder = new ViewHolder(view);
        holder.title.setText(currLight.name);
        holder.button.setEnabled(!currLight.added);
    }

    class LightsAdapter extends PagerAdapter {
        LayoutInflater inflater = null;

        @Override
        public int getCount() {
            return mLights.size();
        }

        @Override
        public boolean isViewFromObject(View view, Object o) {
            return view == o;
        }

        @Override
        public void destroyItem(ViewGroup container, int position, Object object) {
            container.removeView((View) object);
        }

        @Override
        public Object instantiateItem(ViewGroup container, int position) {
            View view;
            if (inflater == null) {
                inflater = (LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE);
            }
            Light light = mLights.get(position);
            view = inflater.inflate(R.layout.light_cover_flow_item, container, false);
            view.setTag(light);
            ViewHolder holder = new ViewHolder(view);
            holder.title.setText(light.name);
            holder.description.setText(getString(R.string.light_model_format, light.model));
            holder.button.setTag(light);
            holder.button.setOnClickListener(AddLightsActivity.this);
            holder.button.setEnabled(!light.added);
            container.addView(view);
            return view;
        }
    }


    private static class ViewHolder {

        public ImageView image;

        public TextView title;

        public TextView description;

        public Button button;

        public ViewHolder(View v) {
            image = (ImageView) v.findViewById(R.id.light_image);
            title = (TextView) v.findViewById(R.id.light_title);
            description = (TextView) v.findViewById(R.id.light_desc);
            button = (Button) v.findViewById(R.id.light_button);
        }
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
}