package com.youwill.store.activities;

import com.nostra13.universalimageloader.core.ImageLoader;
import com.youwill.store.R;
import com.youwill.store.utils.Constants;

import android.app.Activity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;

import uk.co.senab.photoview.PhotoView;
import uk.co.senab.photoview.PhotoViewAttacher;

/**
 * Created by tian on 14/12/25:下午8:39.
 */
public class ViewImageActivity extends Activity {

    PhotoView mScaleImageView;

    ImageLoader mImageLoader;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTheme(R.style.ImageViewTheme);
        setContentView(R.layout.activity_view_image);
        mImageLoader = ImageLoader.getInstance();
        mScaleImageView = (PhotoView) findViewById(R.id.image);
        String url = getIntent().getStringExtra(Constants.INTENT_EXTRA_URL);
        if (url != null) {
            mImageLoader.displayImage(url, mScaleImageView);
        }

        mScaleImageView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                finish();
            }
        });
        mScaleImageView.setOnViewTapListener(new PhotoViewAttacher.OnViewTapListener() {
            @Override
            public void onViewTap(View view, float x, float y) {
                finish();
            }
        });
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        finish();
        return true;
    }


}