package com.kii.yankon.activities;

import android.app.ListActivity;
import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CompoundButton;

import com.kii.yankon.R;
import com.kii.yankon.model.Light;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.services.NetworkSenderService;
import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.Global;
import com.kii.yankon.widget.LightItemViewHolder;

import java.util.ArrayList;
import java.util.List;

public class AddLights2Activity extends ListActivity implements CompoundButton.OnCheckedChangeListener {

    private List<Light> mLights = new ArrayList<Light>();
    private LightsAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_lights2);

        getActionBar().setHomeButtonEnabled(true);
        getActionBar().setDisplayHomeAsUpEnabled(true);

        mAdapter = new LightsAdapter(this);

        Light l = new Light();
        l.name = "Unknown1";
        l.model = "model1";
        l.mac = "11";
        Global.gLightsMacMap.put(l.mac, l);
        addLightToList(l);
        l = new Light();
        l.name = "Unknown2";
        l.model = "model1";
        l.mac = "22";
        Global.gLightsMacMap.put(l.mac, l);
        addLightToList(l);
        l = new Light();
        l.name = "Unknown3";
        l.model = "model1";
        l.mac = "33";
        Global.gLightsMacMap.put(l.mac, l);
        addLightToList(l);
        l = new Light();
        l.name = "Unknown4";
        l.model = "model1";
        l.mac = "44";
        Global.gLightsMacMap.put(l.mac, l);
        addLightToList(l);
        l = new Light();
        l.name = "Unknown5";
        l.model = "model1";
        l.mac = "55";
        Global.gLightsMacMap.put(l.mac, l);
        addLightToList(l);

        setListAdapter(mAdapter);

        IntentFilter filter = new IntentFilter(Constants.ACTION_LIGHT_UPDATED);
        LocalBroadcastManager.getInstance(this).registerReceiver(mReceiver, filter);
        NetworkSenderService.sendCmd(this, (String) null, Constants.SEARCH_LIGHTS_CMD);
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();
        LocalBroadcastManager.getInstance(this).unregisterReceiver(mReceiver);
    }

    BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (Global.gLightsMacMap) {
                for (Light light : Global.gLightsMacMap.values()) {
                    addLightToList(light);
                }
            }
        }
    };

    void addLightToList(Light light) {
        if (mLights.contains(light)) {
            return;
        }
        Cursor cursor = getContentResolver().query(YanKonProvider.URI_LIGHTS, null, "MAC=(?) AND deleted=0", new String[]{light.mac}, null);
        if (cursor.moveToFirst()) {
            light.name = cursor.getString(cursor.getColumnIndex("name"));
            light.model = cursor.getString(cursor.getColumnIndex("model"));
            light.added = true;
        }
        cursor.close();
        mLights.add(light);
        mAdapter.notifyDataSetChanged();
    }

    void addLightToDB(Light light) {
        Log.e("Evan", "Add light:" + light.mac);
        ContentValues values = new ContentValues();
        values.put("MAC", light.mac);
        values.put("model", light.model);
        values.put("connected", true);
        values.put("state", light.state);
        values.put("IP", light.ip);
        values.put("color", light.color);
        values.put("brightness", Math.max(light.brightness, Constants.MIN_BRIGHTNESS));
        values.put("CT", Math.max(light.CT, Constants.MIN_CT));
        values.put("name", light.name);
        values.put("owned_time", System.currentTimeMillis());
        values.put("deleted", false);
        getContentResolver().insert(YanKonProvider.URI_LIGHTS, values);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.add_lights2, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                break;
            case R.id.action_select_all: {
                boolean isAllSelected = true;
                for (Light l : mLights) {
                    if (!l.added) {
                        if (!l.selected) {
                            l.selected = true;
                            isAllSelected = false;
                        }
                    }
                }
                if (isAllSelected) {
                    for (Light l : mLights) {
                        if (!l.added) {
                            l.selected = false;
                        }
                    }
                }
                mAdapter.notifyDataSetChanged();
            }
            break;
            case R.id.action_done: {
                for (Light l : mLights) {
                    if (!l.added && l.selected) {
                        addLightToDB(l);
                    }
                }
                finish();
            }
            break;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        Light light = (Light) buttonView.getTag();
        if (light != null) {
            light.selected = isChecked;
        }
    }

    class LightsAdapter extends BaseAdapter {
        Context mContext = null;

        LightsAdapter(Context context) {
            super();
            mContext = context;
        }

        @Override
        public int getCount() {
            return mLights.size();
        }

        @Override
        public Object getItem(int position) {
            return mLights.get(position);
        }

        @Override
        public long getItemId(int position) {
            return 0;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View view = convertView;
            if (view == null) {
                view = LayoutInflater.from(mContext).inflate(R.layout.light_item, parent, false);
                LightItemViewHolder holder = new LightItemViewHolder(view);
                view.setTag(holder);
            }
            Light light = (Light) getItem(position);
            LightItemViewHolder holder = (LightItemViewHolder) view.getTag();
            holder.textView1.setText(light.name);
            holder.textView2.setText(light.model);
            holder.switchButton.setChecked(light.state);
            holder.checkBox.setChecked(light.selected || light.added);
            holder.checkBox.setEnabled(!light.added);
            holder.checkBox.setVisibility(View.VISIBLE);
            holder.checkBox.setOnCheckedChangeListener(AddLights2Activity.this);
            holder.checkBox.setTag(light);
            return view;
        }
    }
}
