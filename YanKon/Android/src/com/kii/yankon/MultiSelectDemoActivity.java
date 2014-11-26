package com.kii.yankon;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.util.SparseBooleanArray;
import android.view.ActionMode;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;


public class MultiSelectDemoActivity extends Activity
        implements ActionMode.Callback, AdapterView.OnItemClickListener,
        AdapterView.OnItemLongClickListener {

    private SparseBooleanArray mCheckedMap = new SparseBooleanArray();

    private DemoListAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_multi_select_demo);
        ListView listView = (ListView) findViewById(R.id.list);
        mAdapter = new DemoListAdapter();
        listView.setAdapter(mAdapter);
        listView.setOnItemClickListener(this);
        listView.setOnItemLongClickListener(this);
    }

    private ActionMode mActionMode;

    private static final String[] GENRES = new String[]{
            "Action", "Adventure", "Animation", "Children", "Comedy", "Documentary", "Drama",
            "Foreign", "History", "Independent", "Romance", "Sci-Fi", "Television", "Thriller"
    };

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        mode.getMenuInflater().inflate(R.menu.menu_multi_select_demo, menu);
        mode.setTitle("Delete mode");
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
                        String.format("%d items will be deleted!", getSelectedCount()))
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
        clearSelection();
        mActionMode = null;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        if (mActionMode == null) {
            /*no items selected, so perform item click actions
             * like moving to next activity */
            Toast toast = Toast.makeText(getApplicationContext(), "Item "
                            + (position + 1) + ": " + mAdapter.getItem(position),
                    Toast.LENGTH_SHORT);
            toast.setGravity(Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 0, 0);
            toast.show();

        } else {
            onListItemSelected(position);
        }
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        onListItemSelected(position);
        return true;
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
        final int checkedCount = mCheckedMap.size();
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

    private void switchChecked(int position, boolean checked) {
        if (checked) {
            mCheckedMap.put(position, true);
        } else {
            mCheckedMap.delete(position);
        }
        mAdapter.notifyDataSetChanged();
    }

    private void toggleSelection(int position) {
        switchChecked(position, !mCheckedMap.get(position));
    }

    private void clearSelection() {
        mCheckedMap = new SparseBooleanArray();
        mAdapter.notifyDataSetChanged();
    }

    private int getSelectedCount() {
        return mCheckedMap.size();
    }

    private void onListItemSelected(int position) {
        toggleSelection(position);
        boolean hasCheckedItems = getSelectedCount() > 0;

        if (hasCheckedItems && mActionMode == null) {
            mActionMode = startActionMode(this);
        } else if (!hasCheckedItems && mActionMode != null) {
            mActionMode.finish();
        }

        if (mActionMode != null) {
            setSubtitle(mActionMode);
        }
    }
}
