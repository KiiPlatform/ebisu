package com.youwill.store.view;

import android.content.Context;
import android.content.Intent;
import android.text.style.ClickableSpan;
import android.view.View;

import com.youwill.store.activities.LogInActivity;

/**
 * Created by Evan on 14/10/23.
 */
public class LoginSpan extends ClickableSpan {
    @Override
    public void onClick(View widget) {
        Context context = widget.getContext();
        Intent intent = new Intent(context, LogInActivity.class);
        context.startActivity(intent);
    }
}
