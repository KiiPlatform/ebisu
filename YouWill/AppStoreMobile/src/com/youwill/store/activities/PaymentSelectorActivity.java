package com.youwill.store.activities;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import com.kii.payment.PayType;
import com.youwill.store.R;
import com.youwill.store.utils.Constants;
import com.youwill.store.utils.Settings;
import com.youwill.store.utils.Utils;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by tian on 14/12/3:上午11:53.
 */
public class PaymentSelectorActivity extends Activity
        implements View.OnClickListener {

    RadioGroup mRadioGroup;

    public static final String SHOULD_DIM_BEHIND = "should_dim_behind";


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
        initViews();
    }

    void initViews() {
        mRadioGroup = (RadioGroup) findViewById(R.id.payment_group);
        PayType payType = Settings.getLastUsedPayType(this);
        RadioButton rbAlipay = ((RadioButton) findViewById(R.id.alipay_button));
        RadioButton rbMM = (RadioButton) findViewById(R.id.mm_button);
        RadioButton rbUnion = (RadioButton) findViewById(R.id.unionpay_button);
        rbAlipay.setOnClickListener(this);
        rbMM.setOnClickListener(this);
        rbUnion.setOnClickListener(this);
        switch (payType) {
            case alipay:
                rbAlipay.setChecked(true);
                break;
            case mm:
                rbMM.setChecked(true);
                break;
            case unionpay:
                rbUnion.setChecked(true);
                break;
        }
    }

    @Override
    public void onClick(View v) {
        Intent intent = new Intent();
        switch (v.getId()) {
            case R.id.alipay_button:
                intent.putExtra(Constants.INTENT_EXTRA_PAY_TYPE, PayType.alipay.name());
                break;
            case R.id.mm_button:
                intent.putExtra(Constants.INTENT_EXTRA_PAY_TYPE, PayType.mm.name());
                break;
            case R.id.unionpay_button:
                intent.putExtra(Constants.INTENT_EXTRA_PAY_TYPE, PayType.unionpay.name());
                break;
            default:
                intent.putExtra(Constants.INTENT_EXTRA_PAY_TYPE, PayType.alipay);
                break;
        }
        setResult(RESULT_OK, intent);
        finish();
    }
}
