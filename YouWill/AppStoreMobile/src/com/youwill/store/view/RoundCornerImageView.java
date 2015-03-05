package com.youwill.store.view;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.widget.ImageView;

/**
 * Created by tian on 14-10-5:上午11:07.
 */
public class RoundCornerImageView  extends ImageView{
    private Paint paint = new Paint();
    private Matrix matrix = new Matrix();
    private static final String TAG = RoundCornerImageView.class.getName();

    public RoundCornerImageView(Context context) {
        super(context);
    }

    public RoundCornerImageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public RoundCornerImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (getDrawable() != null) {
            BitmapShader shader;
            BitmapDrawable bitmapDrawable = (BitmapDrawable) getDrawable();
            Bitmap roundBitmap = bitmapDrawable.getBitmap();
            if (roundBitmap.getWidth() != roundBitmap.getHeight()) {
                int roundSize = roundBitmap.getWidth() > roundBitmap.getHeight() ? roundBitmap.getHeight() : roundBitmap.getWidth();
                roundBitmap = Bitmap.createScaledBitmap(roundBitmap, roundSize, roundSize, true);
            }
            shader = new BitmapShader(roundBitmap,
                    Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
            RectF rect = new RectF(0.0f+getPaddingLeft(), 0.0f+getPaddingLeft(), getWidth(), getHeight());
            int width = roundBitmap.getWidth();
            int height = roundBitmap.getHeight();
            RectF src = new RectF(0.0f, 0.0f, width, height);
            matrix.setRectToRect(src, rect, Matrix.ScaleToFit.CENTER);
            shader.setLocalMatrix(matrix);
            paint.setAntiAlias(true);
            paint.setShader(shader);
            canvas.drawRoundRect(rect, 10, 10, paint);
        }else {
            super.onDraw(canvas);
        }
    }


}
