package com.youwill.store.activities;

import com.youwill.store.R;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.WindowManager;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by tian on 14/12/3:上午11:53.
 */
public class PaymentSelectorActivity extends Activity
        implements RadioGroup.OnCheckedChangeListener {

    RadioGroup mRadioGroup;

    public static final String PAYMENT_METHOD = "payment_method";

    public static final String SHOULD_DIM_BEHIND = "should_dim_behind";

    public static final int PAYMENT_ALIPAY = 0;

    public static final int PAYMENT_UNIONPAY = 1;

    public static final int PAYMENT_PAYPAL = 2;

    private Map<Integer, Integer> mIdMap = new HashMap<Integer, Integer>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_payment_selector);
        boolean shouldDimBehind = getIntent().getBooleanExtra(SHOULD_DIM_BEHIND, false);
        if (shouldDimBehind) {
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND,
                    WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        }
        getWindow().setLayout(585, 556);
        mRadioGroup = (RadioGroup) findViewById(R.id.payment_group);
        mIdMap.put(R.id.alipay_button, PAYMENT_ALIPAY);
        mIdMap.put(R.id.unionpay_button, PAYMENT_UNIONPAY);
        mIdMap.put(R.id.paypal_button, PAYMENT_PAYPAL);
        int payment = getIntent().getIntExtra(PAYMENT_METHOD, PAYMENT_ALIPAY);
        for (int id : mIdMap.keySet()) {
            if (mIdMap.get(id) == payment) {
                RadioButton button = (RadioButton) findViewById(id);
                if (button != null) {
                    button.setChecked(true);
                }
            }
        }
        mRadioGroup.setOnCheckedChangeListener(this);
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        Intent intent = new Intent();
        intent.putExtra(PAYMENT_METHOD, mIdMap.get(checkedId));
        setResult(RESULT_OK, intent);
        finish();
    }
}
