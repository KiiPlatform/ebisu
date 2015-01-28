package com.kii.yankon.activities;

import android.app.Activity;
import android.database.Cursor;
import android.os.AsyncTask;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.Toast;

import com.kii.yankon.R;
import com.kii.yankon.fragments.ProgressDialogFragment;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.KiiSync;
import com.kii.yankon.utils.Utils;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

public class ChangePasswordActivity extends Activity {

    public static final String EXTRA_LIGHTS = "lights";
    String[] lightIds;
    String[] lightMacs;
    HashMap<String, String> lightMacNameMap = new HashMap<>();


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_change_password);
        getActionBar().setHomeButtonEnabled(true);
        getActionBar().setDisplayHomeAsUpEnabled(true);
        lightIds = getIntent().getStringArrayExtra(EXTRA_LIGHTS);
        if (lightIds == null || lightIds.length == 0) {
            finish();
            return;
        }
        ArrayList<String> macs = new ArrayList<>();
        Cursor cursor = getContentResolver().query(YanKonProvider.URI_LIGHTS, new String[]{"MAC", "name"},
                "_id in " + Utils.buildNumsInSQL(lightIds), null, null);
        if (cursor != null) {
            while (cursor.moveToNext()) {
                String name = cursor.getString(1);
                String mac = cursor.getString(0);
                macs.add(mac);
                lightMacNameMap.put(mac, name);
            }
            cursor.close();
        }
        lightMacs = macs.toArray(new String[macs.size()]);
    }


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_change_password, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                break;
            case R.id.action_done:
                save();
                break;
        }

        return super.onOptionsItemSelected(item);
    }

    private void save() {
        String oldAdminPwd = ((EditText) findViewById(R.id.old_admin_pwd)).getText().toString();
        String newAdminPwd = ((EditText) findViewById(R.id.new_admin_pwd)).getText().toString();
        String cNewAdminPwd = ((EditText) findViewById(R.id.confirm_new_admin_pwd)).getText().toString();
        String newRemotePwd = ((EditText) findViewById(R.id.new_remote_pwd)).getText().toString();
        String cNewRemotePwd = ((EditText) findViewById(R.id.confirm_new_remote_pwd)).getText().toString();

        if (newAdminPwd.length() + newRemotePwd.length() == 0) {
            Toast.makeText(this, "You need to set one of admin password or remote password", Toast.LENGTH_SHORT).show();
            return;
        }
        if (oldAdminPwd.length() == 0) {
            Toast.makeText(this, "Current admin password is required", Toast.LENGTH_SHORT).show();
            return;
        }
        if (!newAdminPwd.equals(cNewAdminPwd)) {
            Toast.makeText(this, "New admin password is not confirmed", Toast.LENGTH_SHORT).show();
            return;
        }
        if (!newAdminPwd.equals(cNewAdminPwd)) {
            Toast.makeText(this, "New admin password is not confirmed", Toast.LENGTH_SHORT).show();
            return;
        }

        if (newRemotePwd.length() > 0 && (newRemotePwd.length() != 4 || !TextUtils.isDigitsOnly(newRemotePwd))) {
            Toast.makeText(this, "Remote password must be 4 digits", Toast.LENGTH_SHORT).show();
            return;
        }

        if (!newRemotePwd.equals(cNewRemotePwd)) {
            Toast.makeText(this, "New remote password is not confirmed", Toast.LENGTH_SHORT).show();
            return;
        }

        new ChangePwdTask(lightMacs, oldAdminPwd, newAdminPwd, newRemotePwd).execute();
    }

    class ChangePwdTask extends AsyncTask<Void, Void, Void> {

        String oldPwd, adminPwd, remotePwd;
        String result;
        String[] lights;

        ProgressDialogFragment dialogFragment;

        public ChangePwdTask(String[] lights, String oldPwd, String adminPwd, String remotePwd) {
            super();
            this.oldPwd = oldPwd;
            this.adminPwd = adminPwd;
            this.remotePwd = remotePwd;
            this.lights = lights;
        }

        @Override
        protected Void doInBackground(Void... params) {
            result = KiiSync.changePwd(lights, oldPwd, adminPwd, remotePwd);
            return null;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            dialogFragment = ProgressDialogFragment.newInstance(null, getString(R.string.change_pwd_async_msg));
            dialogFragment.show(getFragmentManager(), "dialog");
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            super.onPostExecute(aVoid);
            dialogFragment.dismiss();
            if (TextUtils.isEmpty(result)) {
                Toast.makeText(ChangePasswordActivity.this, R.string.kii_extension_return_null, Toast.LENGTH_SHORT).show();
                return;
            }
            ArrayList<String> succList = new ArrayList<>();
            ArrayList<String> failList = new ArrayList<>();
            try {
                JSONObject root = new JSONObject(result);
                JSONObject succ = root.getJSONObject("success");

                Iterator<String> it = succ.keys();
                while (it.hasNext()) {//遍历JSONObject
                    String mac = it.next();
                    boolean res = succ.getBoolean(mac);
                    if (res) {
                        succList.add(mac);
                    } else {
                        String name = lightMacNameMap.get(mac);
                        if (!TextUtils.isEmpty(name)) {
                            failList.add(name);
                        }
                    }
                }
            } catch (Exception e) {
                Log.e(ChangePasswordActivity.class.getSimpleName(), Log.getStackTraceString(e));
            }
            if (succList.size() == lightMacs.length) {
                Toast.makeText(ChangePasswordActivity.this, "Change password successfully", Toast.LENGTH_SHORT).show();
                finish();
            } else if (succList.size() > 0) {
                StringBuilder sb = new StringBuilder();
                for (String s : failList) {
                    if (sb.length() > 0) {
                        sb.append(", ");
                    }
                    sb.append(s);
                }
                String txt = succList.size() + " light(s) change password successfully. The following lights change password failed: " + sb.toString();
                Toast.makeText(ChangePasswordActivity.this, txt, Toast.LENGTH_LONG).show();
                finish();
            } else {
                Toast.makeText(ChangePasswordActivity.this, "Change password all failed, please check current admin password", Toast.LENGTH_LONG).show();
            }

        }
    }
}
