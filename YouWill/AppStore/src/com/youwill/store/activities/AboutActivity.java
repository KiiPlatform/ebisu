package com.youwill.store.activities;

import com.kii.cloud.storage.Kii;
import com.kii.cloud.storage.KiiObject;
import com.kii.cloud.storage.query.KiiClause;
import com.kii.cloud.storage.query.KiiQuery;
import com.kii.cloud.storage.query.KiiQueryResult;
import com.youwill.store.R;
import com.youwill.store.utils.DataUtils;
import com.youwill.store.utils.LogUtils;
import com.youwill.store.utils.Utils;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import java.util.List;

/**
 * Created by tian on 14/11/13:下午11:19.
 */
public class AboutActivity extends Activity implements View.OnClickListener {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_about);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().setLayout(585, 556);
        findViewById(R.id.close).setOnClickListener(this);
        findViewById(R.id.check_update).setOnClickListener(this);
        findViewById(R.id.link).setOnClickListener(this);
        TextView version = (TextView) findViewById(R.id.version);
        version.setText(getString(R.string.version_prompt) + Utils.getVersionName(this));
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.close:
                finish();
                break;
            case R.id.check_update:
                checkUpgrade();
                break;
            case R.id.link:
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse("http://www.youwill.com.cn"));
                try {
                    startActivity(intent);
                } catch (Exception ignored) {

                }
                break;
            default:
                break;
        }
    }

    private void checkUpgrade() {
        new Thread() {
            @Override
            public void run() {
                try {
                    KiiClause clause = KiiClause.equals("app_id", Kii.getAppId());
                    LogUtils.d("search clause is " + clause.toString());
                    KiiQuery query = new KiiQuery(clause);
                    KiiQueryResult<KiiObject> result = Kii.bucket("apps")
                            .query(query);
                    List<KiiObject> objLists = result.getResult();
                    LogUtils.d("checkUpgrade, result size: " + objLists.size());
                    if (objLists != null && !objLists.isEmpty()) {
                        DataUtils.storeAppsToDB(AboutActivity.this, objLists);
                        KiiObject object = objLists.get(0);
                        int cloudVersionCode = object.getInt("version_code");
                        int localVersionCode = Utils.getVersionCode(AboutActivity.this);
                        if (cloudVersionCode > localVersionCode) {
                            mHandler.sendEmptyMessage(MSG_CHECK_UPGRADE_AVAILABLE);
                        } else {
                            mHandler.sendEmptyMessage(MSG_CHECK_NO_UPGRADE);
                        }
                    } else {
                        mHandler.sendEmptyMessage(MSG_CHECK_NO_UPGRADE);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    mHandler.sendEmptyMessage(MSG_CHECK_NETWORK_ERROR);
                }
            }
        }.start();
        Utils.showProgressDialog(this, getString(R.string.check_update));
    }

    private static final int MSG_CHECK_NO_UPGRADE = 0;

    private static final int MSG_CHECK_UPGRADE_AVAILABLE = 1;

    private static final int MSG_CHECK_NETWORK_ERROR = 2;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            Utils.dismissProgressDialog();
            switch (msg.what) {
                case MSG_CHECK_NETWORK_ERROR:
                    Toast.makeText(AboutActivity.this, R.string.network_unavailable,
                            Toast.LENGTH_SHORT).show();
                    break;
                case MSG_CHECK_NO_UPGRADE:
                    Toast.makeText(AboutActivity.this, R.string.app_store_no_upgrade,
                            Toast.LENGTH_SHORT).show();
                    break;
                case MSG_CHECK_UPGRADE_AVAILABLE:
                    Toast.makeText(AboutActivity.this, R.string.app_store_has_upgrade,
                            Toast.LENGTH_SHORT).show();
                    Intent intent = new Intent(AboutActivity.this, AppDetailActivity.class);
                    intent.putExtra(AppDetailActivity.EXTRA_APP_ID, Kii.getAppId());
                    startActivity(intent);
                    break;
            }
        }
    };
}
