package com.kii.yankon;

import android.app.Activity;
import android.os.Bundle;
import android.support.v4.view.PagerAdapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import com.kii.yankon.model.Light;
import com.pixplicity.multiviewpager.MultiViewPager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;


/**
 * Created by tian on 14/11/24:下午11:50.
 */
public class AddLightsActivity extends Activity implements View.OnClickListener {

    private MultiViewPager mPager;
    private List<Light> mLights = new ArrayList<Light>();
    private LightsAdapter mAdapter;
    private HashMap<String, Light> lightMacMap = new HashMap<String, Light>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_lights);
        mPager = (MultiViewPager) findViewById(R.id.pager);

        mAdapter = new LightsAdapter();

        Light l = new Light();
        l.name = "Unknown1";
        l.model = "Model YZ";
        l.mac = "1";
        addLight(l);
        l = new Light();
        l.name = "Unknown2";
        l.model = "Model YZ";
        l.mac = "2";
        addLight(l);
        l = new Light();
        l.name = "Unknown3";
        l.model = "Model YZ";
        l.mac = "3";
        addLight(l);
        l = new Light();
        l.name = "Unknown4";
        l.model = "Model YZ";
        l.mac = "4";
        addLight(l);
        l = new Light();
        l.name = "Unknown5";
        l.model = "Model YZ";
        l.mac = "5";
        addLight(l);


        mPager.setAdapter(mAdapter);
    }

    void addLight(Light light) {

        mLights.add(light);
        lightMacMap.put(light.mac, light);
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.light_button:

                break;
        }
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
            ViewHolder holder = new ViewHolder(view);
            holder.title.setText(light.name);
            holder.description.setText(light.model);
            holder.button.setTag(light);
            holder.button.setOnClickListener(AddLightsActivity.this);
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
}