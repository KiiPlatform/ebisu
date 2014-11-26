package com.kii.yankon;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;


public class MultiSelectDemoActivity extends Activity
        implements AbsListView.MultiChoiceModeListener, AdapterView.OnItemClickListener,
        AdapterView.OnItemLongClickListener {

    private ListView mListView;

    private static final String TAG = MultiSelectDemoActivity.class.getSimpleName();

    private SparseBooleanArray mCheckedMap = new SparseBooleanArray();

    private DemoListAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_multi_select_demo);
        mListView = (ListView) findViewById(R.id.list);
        mAdapter = new DemoListAdapter();
        mListView.setAdapter(mAdapter);
        mListView.setOnItemClickListener(this);
        mListView.setOnItemLongClickListener(this);
    }

    private ActionMode mActionMode;

    private static final String[] GENRES = new String[]{
            "Action", "Adventure", "Animation", "Children", "Comedy", "Documentary", "Drama",
            "Foreign", "History", "Independent", "Romance", "Sci-Fi", "Television", "Thriller"
    };

    @Override
    public void onItemCheckedStateChanged(ActionMode mode, int position, long id, boolean checked) {
        setSubtitle(mode);
        mCheckedMap.put(position, checked);
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu_multi_select_demo, menu);
        mode.setTitle(R.string.delete_mode);
        setSubtitle(mode);
        return true;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return false;
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_delete:
                new AlertDialog.Builder(this).setTitle(
                        String.format("%d items will be deleted!", mListView.getCheckedItemCount()))
                        .setPositiveButton(android.R.string.ok, null)
                        .setNegativeButton(android.R.string.cancel, null).show();
                break;
            default:
                break;
        }
        return true;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {

    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        Log.d(TAG, "item " + position + " is clicked!");
        if (mActionMode == null) {
            //work as normal
        } else {
            boolean isChecked = !mCheckedMap.get(position);
            handleItemChecked(position, id, isChecked);
        }
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        if (mActionMode != null) {
            return false;
        }
        mListView.setChoiceMode(AbsListView.CHOICE_MODE_MULTIPLE_MODAL);
        mListView.setMultiChoiceModeListener(this);
        mActionMode = startActionMode(this);
        handleItemChecked(position, id, true);
        return true;
    }

    private void handleItemChecked(int position, long id, boolean isChecked) {
        mListView.setItemChecked(position, true);
        onItemCheckedStateChanged(mActionMode, position, id, isChecked);
    }

    private class DemoListAdapter extends BaseAdapter {

        @Override
        public int getCount() {
            return GENRES.length;
        }

        @Override
        public String getItem(int position) {
            return GENRES[position];
        }

        @Override
        public long getItemId(int position) {
            return GENRES[position].hashCode();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View view = convertView;
            if (view == null) {
                view = getLayoutInflater()
                        .inflate(R.layout.demo_multi_select_list_item, parent, false);
            }
            view.setBackgroundColor(
                    mCheckedMap.get(position) ? getResources().getColor(R.color.demo_checked)
                            : getResources().getColor(R.color.demo_unchecked));
            TextView tv = (TextView) view.findViewById(R.id.text1);
            tv.setText(getItem(position));
            tv = (TextView) view.findViewById(R.id.text2);
            tv.setText(getItem(position));
            return view;
        }
    }

    private void setSubtitle(ActionMode mode) {
        final int checkedCount = mListView.getCheckedItemCount();
        switch (checkedCount) {
            case 0:
                mode.setSubtitle(null);
                break;
            case 1:
                mode.setSubtitle("One item selected");
                break;
            default:
                mode.setSubtitle("" + checkedCount + " items selected");
                break;
        }
    }
}
