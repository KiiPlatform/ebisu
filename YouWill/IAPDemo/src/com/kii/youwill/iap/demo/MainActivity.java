
package com.kii.youwill.iap.demo;

import com.kii.cloud.storage.KiiUser;
import com.kii.cloud.storage.callback.KiiUserCallBack;
import com.kii.payment.KiiOrder;
import com.kii.payment.KiiPayment;
import com.kii.payment.KiiPaymentCallback;
import com.kii.payment.KiiProduct;
import com.kii.payment.KiiReceipt;
import com.kii.payment.KiiStore;
import com.kii.payment.YouWillIAPSDK;
import com.kii.youwill.iap.demo.utils.Constants;
import com.kii.youwill.iap.demo.utils.LogUtil;
import com.nostra13.universalimageloader.core.ImageLoader;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.Currency;
import java.util.List;
import java.util.Locale;

public class MainActivity extends Activity implements View.OnClickListener, KiiPaymentCallback,
        AdapterView.OnItemClickListener {

    private static final String TAG = MainActivity.class.getName();

    ImageLoader mLoader;

    KiiPayment currentPayment;

    private ListView mList;

    private List<KiiProduct> products = new ArrayList<KiiProduct>();

    private List<KiiReceipt> receipts = new ArrayList<KiiReceipt>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        YouWillIAPSDK.init(Constants.APP_ID);
        setContentView(R.layout.activity_main);
        mAdapter = new ProductAdapter();
        mList = (ListView) findViewById(R.id.list);
        mList.setAdapter(mAdapter);
        mList.setOnItemClickListener(this);
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
        if (currentPayment != null) {
            currentPayment.handleActivityResult(requestCode, resultCode, data);
        }
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
                        order = new KiiOrder(product, user, Locale.US);
                        currentPayment = new KiiPayment(MainActivity.this, order, mCallback);
                        currentPayment.enableSandboxMode(true);
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
            for (KiiProduct product : products) {
                KiiReceipt receipt = KiiStore
                        .getReceipt(product, KiiUser.getCurrentUser());
                receipts.add(receipt);
            }
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
            for (KiiProduct product : products) {
                LogUtil.log(TAG, "product is " + product.getName());
            }
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
            Currency currency = product.getCurrency();
            NumberFormat format = NumberFormat.getInstance();
            format.setCurrency(currency);
            format.setMaximumFractionDigits(currency.getDefaultFractionDigits());
            format.setMinimumFractionDigits(currency.getDefaultFractionDigits());
            tv1.setText(product.getLocalizedName(Locale.getDefault()) + " : "
                    + currency.getSymbol() + "" + format.format(product.getPrice()));
            tv2.setText((receipt == null ? "Not bought" : "Bought"));
            return view;
        }
    }

    KiiPaymentCallback mCallback = new KiiPaymentCallback() {
        @Override
        public void onSuccess() {
            // TODO
            LogUtil.log(TAG, "payment success for order: " + order);
            Toast.makeText(MainActivity.this, String
                            .format(getString(R.string.buy_success), order.getSubject()),
                    Toast.LENGTH_SHORT).show();
        }

        @Override
        public void onError(int errorCode) {
            LogUtil.log(TAG, "payment error, error code is " + errorCode);
            Toast.makeText(MainActivity.this, KiiPayment
                    .getErrorMessage(MainActivity.this, errorCode), Toast.LENGTH_SHORT).show();
        }
    };

}
