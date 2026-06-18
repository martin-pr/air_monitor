package com.example.airmonitor;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;

import java.util.Collections;
import java.util.List;

public class HistoryGraphView extends View {
    public static final class Point {
        public final long   ts;
        public final double value;
        public Point(long ts, double value) { this.ts = ts; this.value = value; }
    }

    public static final class Series {
        public final List<Point> points;
        public final double      yMin;
        public final double      yMax;
        public final int         color;
        public Series(List<Point> points, double yMin, double yMax, int color) {
            this.points = points;
            this.yMin = yMin;
            this.yMax = yMax;
            this.color = color;
        }
    }

    private final Paint dotPaint   = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint gridPaint  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint hourPaint  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float dotRadius;

    private List<Series> seriesList = Collections.emptyList();
    private long  rangeStart;
    private long  rangeEnd;

    // 4 horizontal gridlines at 100/75/50/25 % of value range (top -> bottom).
    // The bottom baseline (0 %) is the x-axis itself.
    private static final float[] GRID_FRACTIONS = { 1.0f, 0.75f, 0.5f, 0.25f };

    public HistoryGraphView(Context context) { super(context); init(); }
    public HistoryGraphView(Context context, AttributeSet attrs) { super(context, attrs); init(); }
    public HistoryGraphView(Context context, AttributeSet attrs, int defStyle) { super(context, attrs, defStyle); init(); }

    private void init() {
        dotPaint.setStyle(Paint.Style.FILL);
        dotRadius = dp(1.0f);

        gridPaint.setStyle(Paint.Style.STROKE);
        gridPaint.setStrokeWidth(dp(1));
        gridPaint.setColor(0x33000000);

        hourPaint.setStyle(Paint.Style.STROKE);
        hourPaint.setStrokeWidth(dp(0.5f));
        hourPaint.setColor(0x14000000);

        labelPaint.setTextSize(sp(9));
        labelPaint.setColor(0x99000000);
    }

    public void setRange(long startTs, long endTs) {
        this.rangeStart = startTs;
        this.rangeEnd   = endTs;
        invalidate();
    }

    public void setSeries(List<Series> series) {
        this.seriesList = series == null ? Collections.<Series>emptyList() : series;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (rangeEnd <= rangeStart) return;

        float width  = getWidth();
        float height = getHeight();
        float left   = dp(46);   // 3 narrow rotated-label columns + margin
        float right  = width - dp(4);
        float top    = dp(4);
        float bottom = height - dp(14);

        // 24 hourly vertical grid lines
        for (int hour = 0; hour <= 24; hour++) {
            float x = left + (right - left) * hour / 24f;
            canvas.drawLine(x, top, x, bottom, hourPaint);
        }

        // x-axis baseline
        canvas.drawLine(left, bottom, right, bottom, gridPaint);

        // 4 horizontal gridlines, each annotated with one rotated value per series
        // (text rotated -90° so values read bottom-to-top along the y axis)
        float colWidth = dp(13);
        float colsStart = dp(8);
        int savedColor = labelPaint.getColor();
        for (float fraction : GRID_FRACTIONS) {
            float y = bottom - (bottom - top) * fraction;
            canvas.drawLine(left, y, right, y, gridPaint);
            for (int i = 0; i < seriesList.size(); i++) {
                Series s = seriesList.get(i);
                int value = (int)Math.round(s.yMin + (s.yMax - s.yMin) * fraction);
                String text = String.valueOf(value);
                labelPaint.setColor(s.color);
                float colX = colsStart + i * colWidth;
                drawRotatedLabel(canvas, text, colX, y);
            }
        }
        labelPaint.setColor(savedColor);

        // x-axis labels at 00, 06, 12, 18, 24
        for (int hour = 0; hour <= 24; hour += 6) {
            String text = (hour < 10 ? "0" : "") + hour;
            float x = left + (right - left) * hour / 24f;
            float w = labelPaint.measureText(text);
            canvas.drawText(text, x - w / 2f, height - dp(2), labelPaint);
        }

        // shared 0 baseline label
        canvas.drawText("0", dp(2), bottom - dp(2), labelPaint);

        // dots
        double timeSpan = rangeEnd - rangeStart;
        for (Series s : seriesList) {
            dotPaint.setColor(s.color);
            double valueSpan = s.yMax - s.yMin;
            if (valueSpan <= 0) continue;
            for (Point p : s.points) {
                if (p.ts < rangeStart || p.ts > rangeEnd) continue;
                float x = left + (float)((p.ts - rangeStart) / timeSpan) * (right - left);
                float y = bottom - (float)((p.value - s.yMin) / valueSpan) * (bottom - top);
                canvas.drawCircle(x, y, dotRadius, dotPaint);
            }
        }
    }

    private void drawRotatedLabel(Canvas canvas, String text, float pivotX, float pivotY) {
        float textWidth = labelPaint.measureText(text);
        canvas.save();
        canvas.rotate(-90, pivotX, pivotY);
        canvas.drawText(text, pivotX - textWidth / 2f, pivotY + sp(9) * 0.35f, labelPaint);
        canvas.restore();
    }

    private float dp(float v) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private float sp(float v) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, v, getResources().getDisplayMetrics());
    }
}
