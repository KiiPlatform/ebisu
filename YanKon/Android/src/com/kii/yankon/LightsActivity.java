package com.kii.yankon;

import com.kii.yankon.model.Light;
import com.kii.yankon.utils.Utils;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import at.technikum.mti.fancycoverflow.FancyCoverFlow;
import at.technikum.mti.fancycoverflow.FancyCoverFlowAdapter;

/**
 * Created by tian on 14/11/24:下午11:50.
 */
public class LightsActivity extends Activity {

    private LightsAdapter mAdapter;

    private List<Light> mLights = new ArrayList<Light>();

    private FancyCoverFlow mCoverFlow;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_lights);
        mCoverFlow = (FancyCoverFlow) findViewById(R.id.cover_flow);
        mCoverFlow.setSpacing(100);
        mCoverFlow.setActionDistance(FancyCoverFlow.ACTION_DISTANCE_AUTO);
        mCoverFlow.setUnselectedAlpha(1.0f);
        mCoverFlow.setUnselectedSaturation(0.0f);
        mCoverFlow.setUnselectedScale(1f);
        mCoverFlow.setMaxRotation(0);
        mAdapter = new LightsAdapter();
        mCoverFlow.setAdapter(mAdapter);
    }

    private class LightsAdapter extends FancyCoverFlowAdapter {

        @Override
        public View getCoverFlowItem(int position, View reusableView, ViewGroup parent) {
            View v = reusableView;
            ViewHolder holder;
            Context context = LightsActivity.this;

            if (v == null) {
                v = getLayoutInflater().inflate(R.layout.light_cover_flow_item, parent, false);
                holder = new ViewHolder(v);
                v.setTag(holder);
                v.setLayoutParams(
                        new FancyCoverFlow.LayoutParams(
                                (int)Utils.dp2Px(context, 250),
                                (int)Utils.dp2Px(context, 300)));
            } else {
                holder = (ViewHolder) v.getTag();
            }
            //TODO: bindView

            return v;
        }

        @Override
        public int getCount() {
            return 5;
        }

        @Override
        public Object getItem(int position) {
            return null;
        }

        @Override
        public long getItemId(int position) {
            return 0;
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