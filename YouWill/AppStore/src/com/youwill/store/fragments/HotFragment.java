package com.youwill.store.fragments;

import com.youwill.store.R;
import com.youwill.store.view.fancycoverflow.FancyCoverFlow;
import com.youwill.store.view.fancycoverflow.FancyCoverFlowAdapter;

import android.app.Fragment;
import android.os.Bundle;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Created by tian on 14-9-23:下午11:01.
 */
public class HotFragment extends Fragment implements View.OnClickListener {

    RecyclerView latestView;

    RecyclerView goodView;

    FancyCoverFlow coverFlow;

    LinearLayoutManager mLinearLayoutManager;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_hot, container, false);
        initView(view);
        return view;
    }

    private void initView(View view) {
        latestView = (RecyclerView) view.findViewById(R.id.latest_horizontal_view);
        goodView = (RecyclerView) view.findViewById(R.id.good_horizontal_view);
        view.findViewById(R.id.show_all_good).setOnClickListener(this);
        view.findViewById(R.id.show_all_new).setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.show_all_good:
                break;
            case R.id.show_all_new:
                break;
        }
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {

        private ImageView icon;

        private TextView name;

        private TextView size;

        public ViewHolder(View itemView) {
            super(itemView);
            icon = (ImageView) itemView.findViewById(R.id.icon);
            name = (TextView) itemView.findViewById(R.id.name);
            size = (TextView) itemView.findViewById(R.id.size);
        }
    }

    private class CoverFlowAdapter extends FancyCoverFlowAdapter {

        @Override
        public View getCoverFlowItem(int position, View reusableView, ViewGroup parent) {
            return null;
        }

        @Override
        public int getCount() {
            return 0;
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

}
