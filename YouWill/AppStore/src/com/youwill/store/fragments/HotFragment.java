package com.youwill.store.fragments;

import com.nostra13.universalimageloader.core.ImageLoader;
import com.nostra13.universalimageloader.core.assist.FailReason;
import com.nostra13.universalimageloader.core.listener.ImageLoadingListener;
import com.youwill.store.MainActivity;
import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Utils;
import com.youwill.store.view.imagecoverflow.CoverFlowAdapter;
import com.youwill.store.view.imagecoverflow.CoverFlowView;

import org.json.JSONObject;

import android.app.Fragment;
import android.content.AsyncQueryHandler;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.os.Handler;
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

    AppAdapter recommend1Adapter;

    AppAdapter recommend2Adapter;

    private static final String TAG = MainActivity.class.getSimpleName();

    private HomeCoverFlowAdapter mCoverFlowAdapter;



    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_hot, container, false);
        initView(view);
        return view;
    }

    private void initView(View view) {
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
        recommend1Adapter = new AppAdapter(recommend1Items);
        latestView.setAdapter(recommend1Adapter);
        recommend2Adapter = new AppAdapter(recommend2Items);
        goodView.setAdapter(recommend2Adapter);
        coverFlow = (CoverFlowView<HomeCoverFlowAdapter>) view.findViewById(R.id.coverflow);
        mCoverFlowAdapter = new HomeCoverFlowAdapter();
//        coverFlow.setAdapter(mCoverFlowAdapter);
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
        initQueryHandler();
        startQuery();
        getActivity().getContentResolver()
                .registerContentObserver(YouWill.Application.CONTENT_URI, true, mObserver);
    }

    private ContentObserver mObserver = new ContentObserver(new Handler()) {
        @Override
        public void onChange(boolean selfChange) {
            startQuery();
        }
    };

    private void startQuery() {
        mQueryHandler.startQuery(YouWill.Application.RECOMMEND_TYPE_COVER_FLOW, null,
                YouWill.Application.CONTENT_URI, COLUMNS, YouWill.Application.RECOMMEND_TYPE + "=?",
                new String[]{Integer.toString(YouWill.Application.RECOMMEND_TYPE_COVER_FLOW)},
                YouWill.Application.RECOMMEND_WEIGHT);
        mQueryHandler.startQuery(YouWill.Application.RECOMMEND_TYPE_LINE1, null,
                YouWill.Application.CONTENT_URI, COLUMNS, YouWill.Application.RECOMMEND_TYPE + "=?",
                new String[]{Integer.toString(YouWill.Application.RECOMMEND_TYPE_LINE1)},
                YouWill.Application.RECOMMEND_WEIGHT);
        mQueryHandler.startQuery(YouWill.Application.RECOMMEND_TYPE_LINE2, null,
                YouWill.Application.CONTENT_URI, COLUMNS, YouWill.Application.RECOMMEND_TYPE + "=?",
                new String[]{Integer.toString(YouWill.Application.RECOMMEND_TYPE_LINE2)},
                YouWill.Application.RECOMMEND_WEIGHT);
    }

    private static final String[] COLUMNS = new String[]{
            YouWill.Application.APP_ID,         //0
            YouWill.Application.PACKAGE_NAME,    //1
            YouWill.Application.APP_INFO,       //2
    };

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

    private class AppAdapter extends RecyclerView.Adapter<ViewHolder> {

        private List<AppItem> items = new ArrayList<AppItem>();

        public AppAdapter(List<AppItem> items) {
            this.items = items;
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int i) {
            View v = getActivity().getLayoutInflater().inflate(R.layout.application_item, viewGroup,
                    false);
            return new ViewHolder(v);
        }

        @Override
        public void onBindViewHolder(ViewHolder viewHolder, int i) {
            AppItem item = items.get(i);
            try {
                final JSONObject app = new JSONObject(item.json);
                String url = app.getString("icon");
                String name = app.getString("name");
                int size = app.getInt("apk_size");
                ImageLoader.getInstance().displayImage(url, viewHolder.icon);
                viewHolder.name.setText(name);
                viewHolder.size.setText(Utils.getFileSizeString(size));
                viewHolder.icon.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        //TODO: launch app detail activity
                    }
                });
            } catch (Exception ignored) {

            }

        }

        @Override
        public int getItemCount() {
            return items.size();
        }
    }

    private class HomeCoverFlowAdapter extends CoverFlowAdapter {

        @Override
        public int getCount() {
            LogUtils.d(TAG, "getCount!!!");
            return coverFlowItems.size();
        }

        @Override
        public Bitmap getImage(int position) {
            AppItem item = coverFlowItems.get(position);
            try {
                JSONObject app = new JSONObject(item.json);
                final String url = app.optString("recommend_image");
                if (ImageLoader.getInstance().getDiskCache().get(url) != null) {
                    return ImageLoader.getInstance().loadImageSync(url);
                } else {
                    ImageLoader.getInstance().loadImage(url, listener);
                }
            } catch (Exception ignored) {

            }
            //TODO: add default picture for cover flow
            return ((BitmapDrawable) (getResources().getDrawable(R.drawable.cover_flow1)))
                    .getBitmap();
        }
    }

    private class AppItem {

        String appId;

        String packageName;

        String json;
    }

    List<AppItem> coverFlowItems = new ArrayList<AppItem>();

    List<AppItem> recommend1Items = new ArrayList<AppItem>();

    List<AppItem> recommend2Items = new ArrayList<AppItem>();

    private ImageLoadingListener listener = new ImageLoadingListener() {
        @Override
        public void onLoadingStarted(String s, View view) {

        }

        @Override
        public void onLoadingFailed(String s, View view, FailReason failReason) {

        }

        @Override
        public void onLoadingComplete(String s, View view, Bitmap bitmap) {
            mCoverFlowAdapter.notifyDataSetChanged();
        }

        @Override
        public void onLoadingCancelled(String s, View view) {

        }
    };

    private AsyncQueryHandler mQueryHandler;

    private void initQueryHandler() {
        mQueryHandler = new AsyncQueryHandler(
                getActivity().getContentResolver()) {
            @Override
            protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
                switch (token) {
                    case YouWill.Application.RECOMMEND_TYPE_COVER_FLOW:
                        coverFlowItems = parseCursor(cursor);
                        if (!coverFlowItems.isEmpty()) {
                            coverFlow.setAdapter(mCoverFlowAdapter);
                            mCoverFlowAdapter.notifyDataSetChanged();
                        }
                        break;
                    case YouWill.Application.RECOMMEND_TYPE_LINE1:
                        recommend1Items = parseCursor(cursor);
                        recommend1Adapter.notifyDataSetChanged();
                        break;
                    case YouWill.Application.RECOMMEND_TYPE_LINE2:
                        recommend2Items = parseCursor(cursor);
                        recommend2Adapter.notifyDataSetChanged();
                        break;
                }
            }
        };
    }

    private List<AppItem> parseCursor(Cursor cursor) {
        List<AppItem> items = new ArrayList<AppItem>();
        while (cursor.moveToNext()) {
            AppItem item = new AppItem();
            item.appId = cursor.getString(0);
            item.packageName = cursor.getString(1);
            item.json = cursor.getString(2);
        }
        return items;
    }
}
