package com.youwill.store.fragments;

import android.app.ListFragment;
import android.app.LoaderManager;
import android.content.Context;
import android.content.CursorLoader;
import android.content.Loader;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CursorAdapter;
import android.widget.ImageView;
import android.widget.RatingBar;
import android.widget.TextView;

import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.providers.YouWill;
import com.youwill.store.utils.Utils;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by tian on 14-9-23:下午11:02.
 */
public class PurchasedFragment extends ListFragment implements LoaderManager.LoaderCallbacks<Cursor>{

    PurchasedAppAdapter mAdapter;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        view.setBackgroundColor(Color.WHITE);
        return view;
    }

    @Override
    public void onStart() {
        super.onStart();
        mAdapter = new PurchasedAppAdapter(getActivity(), null, CursorAdapter.FLAG_REGISTER_CONTENT_OBSERVER);
        setListAdapter(mAdapter);
        getListView().setDivider(new ColorDrawable(Color.TRANSPARENT));
        getLoaderManager().initLoader(getClass().hashCode(), null, this);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int i, Bundle bundle) {
        return new CursorLoader(getActivity(), YouWill.Purchased.CONTENT_URI, null, null, null, null);
    }

    @Override
    public void onLoadFinished(Loader<Cursor> objectLoader, Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void onLoaderReset(Loader<Cursor> objectLoader) {
        mAdapter.swapCursor(null);
    }

    class PurchasedAppAdapter extends CursorAdapter {
        public PurchasedAppAdapter(Context context, Cursor c, int flags) {
            super(context, c, flags);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(context).inflate(R.layout.app_list_item, parent, false);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            String app_info_str = cursor.getString(cursor.getColumnIndex(YouWill.Application.APP_INFO));
            JSONObject appInfo = null;
            try {
                appInfo = new JSONObject(app_info_str);
            } catch (JSONException e) {
                e.printStackTrace();
            }
            if (appInfo == null) {
                return;
            }
            TextView tv = (TextView) view.findViewById(R.id.app_grid_pos);
            tv.setText(String.valueOf(cursor.getPosition() + 1));
            ImageView iconView = (ImageView) view.findViewById(R.id.app_grid_icon);
            String iconUrl = appInfo.optString("icon");
            ImageLoader.getInstance().displayImage(iconUrl, iconView, Utils.iconDisplayOptions);
            tv = (TextView) view.findViewById(R.id.app_grid_name);
            tv.setText(appInfo.optString("name"));
            RatingBar bar = (RatingBar) view.findViewById(R.id.app_grid_rate);
            bar.setRating(4);
        }
    }
}
