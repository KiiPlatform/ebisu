package com.youwill.store.fragments;

import com.youwill.store.R;
import com.youwill.store.view.fancycoverflow.FancyCoverFlow;
import com.youwill.store.view.fancycoverflow.FancyCoverFlowAdapter;

import android.app.Fragment;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/**
 * Created by tian on 14-9-23:下午11:01.
 */
public class HotFragment extends Fragment implements View.OnClickListener {

    RecyclerView latestView;

    RecyclerView goodView;

    FancyCoverFlow coverFlow;

    LinearLayoutManager mLinearLayoutManager;
    LinearLayoutManager mLinearLayoutManager2;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_hot, container, false);
        initView(view);
        return view;
    }

    private void initView(View view) {
        applications = getActivity().getPackageManager()
                .getInstalledApplications(PackageManager.GET_META_DATA);
        mLinearLayoutManager = new LinearLayoutManager(getActivity());
        mLinearLayoutManager.setOrientation(LinearLayoutManager.HORIZONTAL);
        mLinearLayoutManager2 = new LinearLayoutManager(getActivity());
        mLinearLayoutManager2.setOrientation(LinearLayoutManager.HORIZONTAL);
        latestView = (RecyclerView) view.findViewById(R.id.latest_horizontal_view);
        goodView = (RecyclerView) view.findViewById(R.id.good_horizontal_view);
        view.findViewById(R.id.show_all_good).setOnClickListener(this);
        view.findViewById(R.id.show_all_new).setOnClickListener(this);
        latestView = (RecyclerView) view.findViewById(R.id.latest_horizontal_view);
        goodView = (RecyclerView) view.findViewById(R.id.good_horizontal_view);
        goodView.setLayoutManager(mLinearLayoutManager2);
        latestView.setLayoutManager(mLinearLayoutManager);
        latestView.setAdapter(new AppAdapter());
        goodView.setAdapter(new AppAdapter());
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

    private List<ApplicationInfo> applications = new ArrayList<ApplicationInfo>();

    private class AppAdapter extends RecyclerView.Adapter<ViewHolder> {

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int i) {
            View v = getActivity().getLayoutInflater().inflate(R.layout.application_item, viewGroup,
                    false);
            ViewHolder vh = new ViewHolder(v);
            return vh;
        }

        @Override
        public void onBindViewHolder(ViewHolder viewHolder, int i) {
            Context context = getActivity();
            ApplicationInfo info = applications.get(i);
            viewHolder.icon.setImageDrawable(context.getPackageManager().getApplicationIcon(info));
            viewHolder.name.setText(context.getPackageManager().getApplicationLabel(info));
            viewHolder.size.setText("100K");
        }

        @Override
        public int getItemCount() {
            return applications.size();
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
