package com.kii.youwill.iap.demo;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.view.*;
import android.widget.*;
import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;
import com.kii.payment.*;
import com.kii.youwill.iap.demo.utils.Constants;
import com.kii.youwill.iap.demo.utils.LogUtil;
import com.nostra13.universalimageloader.core.ImageLoader;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends Activity implements View.OnClickListener, KiiPaymentCallback,
        AdapterView.OnItemClickListener {

    private static final String TAG = MainActivity.class.getName();

    ImageLoader mLoader;

    KiiPayment currentPayment;

    private ListView mList;

    private List<KiiProduct> products = new ArrayList<KiiProduct>();

    private List<KiiReceipt> receipts = new ArrayList<KiiReceipt>();

    private PayType mPayType = PayType.mm;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        YouWillIAPSDK.init("YouWill", Constants.APP_ID);
        setContentView(R.layout.activity_main);
        mAdapter = new ProductAdapter();
        mList = (ListView) findViewById(R.id.list);
        mList.setAdapter(mAdapter);
        mList.setOnItemClickListener(this);

        registerForContextMenu(mList);

        mLoader = ImageLoader.getInstance();
        new GetProductTask(this).execute();

    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        getMenuInflater().inflate(R.menu.selectpayment, menu);
        switch (mPayType) {
            case alipay:
                menu.findItem(R.id.action_pay_alipay).setChecked(true);
                break;
            case mm:
                menu.findItem(R.id.action_pay_mm).setChecked(true);
                break;
            default:
                break;

        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_pay_alipay:
                mPayType = PayType.alipay;
                break;
            case R.id.action_pay_mm:
                mPayType = PayType.mm;
                break;
        }
        return super.onContextItemSelected(item);
    }

    @Override
    public boolean onMenuItemSelected(int featureId, MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_log_in:
                if (TextUtils.isEmpty(Settings.getToken(this))) {
                    Intent intent = new Intent(this, LogInActivity.class);
                    startActivity(intent);
                } else {
                    KiiUser.loginWithToken(new KiiUserCallBack() {
                        @Override
                        public void onLoginCompleted(int token, KiiUser user, Exception exception) {
                            LogUtil.log(TAG, "log in complete: " + token + ", " + user);
                            new GetReceiptsTask(MainActivity.this).execute();
                        }
                    }, Settings.getToken(this));
                }

                return true;
        }
        return super.onMenuItemSelected(featureId, item);
    }

    private KiiOrder order;

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void onClick(View v) {
    }

    private ProgressDialog mProgress;

    @Override
    public void onSuccess() {
        LogUtil.log(TAG, "onSuccess, order is " + order.toString());
    }

    @Override
    public void onError(int errorCode) {
        LogUtil.log(TAG, "onError, errorCode is " + errorCode);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        if (TextUtils.isEmpty(Settings.getToken(this))) {
            Intent intent = new Intent(this, LogInActivity.class);
            startActivity(intent);
        } else {
            // get transaction parameters;
            final KiiProduct product = products.get(position);
            KiiUser.loginWithToken(new KiiUserCallBack() {
                @Override
                public void onLoginCompleted(int token, KiiUser user, Exception exception) {
                    LogUtil.log(TAG, "log in complete: " + token + ", " + user);
                    if (exception == null) {
                        order = new KiiOrder(product, user, mPayType);
                        currentPayment = KiiPayment.getPayment(MainActivity.this, order, mCallback);
                        currentPayment.pay();
                    } else {
                        // TODO
                        LogUtil.log(TAG,
                                "exception while logging in: " + exception.getLocalizedMessage());
                        Toast.makeText(MainActivity.this, R.string.log_in_failed,
                                Toast.LENGTH_SHORT).show();
                    }
                }
            }, Settings.getToken(this));
        }
    }

    private class GetReceiptsTask extends AsyncTask<Void, Void, Void> {

        private Context context;

        public GetReceiptsTask(Context context) {
            super();
            this.context = context;
        }

        @Override
        protected Void doInBackground(Void... params) {
            // TODO Auto-generated method stub
            KiiStore.listReceipts(null, KiiUser.getCurrentUser());
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            super.onPostExecute(result);
            if (mProgress != null) {
                mProgress.dismiss();
            }
            mProgress = null;
            mAdapter.notifyDataSetChanged();
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            mProgress = new ProgressDialog(context);
            mProgress.setTitle("Please wait...");
            mProgress.show();
        }

    }

    private class GetProductTask extends AsyncTask<Void, Void, Void> {

        private Context context;

        public GetProductTask(Context context) {
            super();
            this.context = context;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            mProgress = new ProgressDialog(context);
            mProgress.setTitle("Please wait...");
            mProgress.show();
        }

        @Override
        protected Void doInBackground(Void... params) {
            products.addAll(KiiStore.listProducts(null));
            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            super.onPostExecute(aVoid);
            if (mProgress != null) {
                mProgress.dismiss();
            }
            mProgress = null;
            mAdapter.notifyDataSetChanged();
        }

    }

    private ProductAdapter mAdapter;

    private class ProductAdapter extends BaseAdapter {

        @Override
        public int getCount() {
            return products.size();
        }

        @Override
        public KiiProduct getItem(int position) {
            return products.get(position);
        }

        @Override
        public long getItemId(int position) {
            return 0;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View view = convertView;
            if (view == null) {
                view = LayoutInflater.from(MainActivity.this).inflate(R.layout.product_list_item,
                        parent, false);
            }
            KiiProduct product = getItem(position);
            KiiReceipt receipt = null;
            for (KiiReceipt r : receipts) {
                if (r.getProductId().contentEquals(product.getId())) {
                    receipt = r;
                    break;
                }
            }
            ImageView iv = (ImageView) view.findViewById(R.id.image);
            mLoader.displayImage(product.getThumbnail(), iv);
            TextView tv1 = (TextView) view.findViewById(R.id.text1);
            TextView tv2 = (TextView) view.findViewById(R.id.text2);
            tv1.setText(product.getName() + " ￥" + product.getPrice());
            tv2.setText((receipt == null ? "Not bought" : "Bought"));
            return view;
        }
    }

    KiiPaymentCallback mCallback = new KiiPaymentCallback() {
        @Override
        public void onSuccess() {
            // TODO
            LogUtil.log(TAG, "payment success for order: " + order);
            Message msg = handler.obtainMessage();
            msg.obj = String.format(getString(R.string.buy_success), order.getSubject());
            handler.sendMessage(msg);
        }

        @Override
        public void onError(int errorCode) {
            LogUtil.log(TAG, "payment error, error code is " + errorCode);
            Message msg = handler.obtainMessage();
            msg.obj = KiiPayment.getErrorMessage(MainActivity.this, errorCode);
            handler.sendMessage(msg);
        }
    };

    Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            Toast.makeText(MainActivity.this, (String) msg.obj, Toast.LENGTH_SHORT).show();
        }
    };
}
