package com.kii.yankon;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

/**
 * Created by Evan on 14/11/23.
 */
public class GuideActivity extends Activity implements View.OnClickListener {
    ViewPager mViewPager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mViewPager = new ViewPager(this);
        setContentView(mViewPager);
        mViewPager.setAdapter(new MyAdapter());
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.guide_skip:
                startActivity(new Intent(this, MainActivity.class));
                finish();
                break;
        }
    }

    class MyAdapter extends PagerAdapter {
        LayoutInflater inflater = null;

        @Override
        public int getCount() {
            return 3;
        }

        @Override
        public boolean isViewFromObject(View view, Object o) {
            return o == view;
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
            view = inflater.inflate(R.layout.guide_content, container, false);
            TextView tv = (TextView) view.findViewById(R.id.guide_text);
            tv.setText("App Intro (" + String.valueOf(position + 1) + "/3)");
            Button button = (Button) view.findViewById(R.id.guide_skip);
            if (position == 2) {
                button.setText(getString(R.string.guide_done));
            } else {
                button.setText(getString(R.string.guide_skip));
            }
            button.setOnClickListener(GuideActivity.this);
            ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            container.addView(view, params);
            return view;
        }
    }
}
