package com.youwill.store.fragments;

import com.youwill.store.MainActivity;
import com.youwill.store.R;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.view.imagecoverflow.CoverFlowAdapter;
import com.youwill.store.view.imagecoverflow.CoverFlowView;

import android.app.Fragment;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
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

    LinearLayoutManager mLinearLayoutManager;

    LinearLayoutManager mLinearLayoutManager2;

    CoverFlowView coverFlow;

    private static final String TAG = MainActivity.class.getSimpleName();

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
        coverFlow = (CoverFlowView<HomeCoverFlowAdapter>)view.findViewById(R.id.coverflow);
        HomeCoverFlowAdapter adapter = new HomeCoverFlowAdapter();
        coverFlow.setAdapter(adapter);
        coverFlow.setCoverFlowListener(new CoverFlowView.CoverFlowListener() {
            @Override
            public void imageOnTop(CoverFlowView coverFlowView, int position, float left, float top,
                    float right, float bottom) {

            }

            @Override
            public void topImageClicked(CoverFlowView coverFlowView, int position) {
                LogUtils.d(TAG, "topImageCLicked: " + position);
            }

            @Override
            public void invalidationCompleted() {

            }
        });
        coverFlow.setReflectionHeight(0);
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

    private class HomeCoverFlowAdapter extends CoverFlowAdapter {

        @Override
        public int getCount() {
            LogUtils.d(TAG, "getCount!!!");
            return 3;
        }

        @Override
        public Bitmap getImage(int position) {
            LogUtils.d(TAG, "getImage for position: " + position);
            switch (position % 3) {
                case 0:
                    return ((BitmapDrawable) (getResources().getDrawable(R.drawable.cover_flow1)))
                            .getBitmap();
                case 1:
                    return ((BitmapDrawable) (getResources().getDrawable(R.drawable.cover_flow2)))
                            .getBitmap();
                case 2:
                    return ((BitmapDrawable) (getResources().getDrawable(R.drawable.cover_flow3)))
                            .getBitmap();
            }
            return ((BitmapDrawable) (getResources().getDrawable(R.drawable.cover_flow1)))
                    .getBitmap();
        }
    }

}
