package com.kii.yankon.widget;

import android.util.SparseBooleanArray;
import android.widget.BaseAdapter;

/**
 * Created by tian on 14/11/26:上午11:21.
 */
public abstract class BaseMultiSelectAdapter extends BaseAdapter {

    protected SparseBooleanArray mCheckedMap = new SparseBooleanArray();

    public void switchChecked(int position, boolean checked) {
        if (checked) {
            mCheckedMap.put(position, true);
        } else {
            mCheckedMap.delete(position);
        }
        notifyDataSetChanged();
    }

    public void toggleSelection(int position) {
        switchChecked(position, !mCheckedMap.get(position));
    }

    public void clearSelection() {
        mCheckedMap = new SparseBooleanArray();
        notifyDataSetChanged();
    }

    public int getSelectedCount() {
        return mCheckedMap.size();
    }
}
