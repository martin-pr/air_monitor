package com.example.airmonitor;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

import java.util.Collections;
import java.util.List;
import java.util.TimeZone;

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
    private long rangeStart;
    private long rangeEnd;
    private long viewportStart;
    private long viewportEnd;

    private static final long MIN_VIEWPORT_MS = 5 * 60 * 1000L;  // can't zoom in past 5 minutes
    private static final long HOUR_MS = 60 * 60 * 1000L;
    private static final long MIN_MS  = 60 * 1000L;

    private static final float[] GRID_FRACTIONS = { 1.0f, 0.75f, 0.5f, 0.25f };

    private ScaleGestureDetector scaleDetector;
    private GestureDetector       gestureDetector;

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

        setClickable(true);
        scaleDetector   = new ScaleGestureDetector(getContext(), new ScaleListener());
        gestureDetector = new GestureDetector(getContext(), new GestureListener());
    }

    public void setRange(long startTs, long endTs) {
        if (this.rangeStart != startTs || this.rangeEnd != endTs) {
            this.rangeStart = startTs;
            this.rangeEnd   = endTs;
            this.viewportStart = startTs;
            this.viewportEnd   = endTs;
        }
        invalidate();
    }

    public void setSeries(List<Series> series) {
        this.seriesList = series == null ? Collections.<Series>emptyList() : series;
        invalidate();
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        scaleDetector.onTouchEvent(ev);
        if (!scaleDetector.isInProgress()) {
            gestureDetector.onTouchEvent(ev);
        }
        getParent().requestDisallowInterceptTouchEvent(true);
        return true;
    }

    private float graphLeft()  { return dp(46); }
    private float graphRight() { return getWidth() - dp(4); }

    private class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener {
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            float scaleFactor = detector.getScaleFactor();
            long currentSpan = viewportEnd - viewportStart;
            long newSpan = (long)(currentSpan / scaleFactor);
            newSpan = Math.max(MIN_VIEWPORT_MS, Math.min(rangeEnd - rangeStart, newSpan));

            float left  = graphLeft();
            float right = graphRight();
            float ratio = right > left ? (detector.getFocusX() - left) / (right - left) : 0.5f;
            ratio = Math.max(0f, Math.min(1f, ratio));

            long focusTs = viewportStart + (long)(currentSpan * ratio);
            long newStart = focusTs - (long)(newSpan * ratio);
            long newEnd   = newStart + newSpan;
            if (newStart < rangeStart) { newStart = rangeStart; newEnd = newStart + newSpan; }
            if (newEnd   > rangeEnd)   { newEnd   = rangeEnd;   newStart = newEnd - newSpan; }

            viewportStart = newStart;
            viewportEnd   = newEnd;
            invalidate();
            return true;
        }
    }

    private class GestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override public boolean onDown(MotionEvent e) { return true; }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float dx, float dy) {
            float left  = graphLeft();
            float right = graphRight();
            if (right <= left) return false;

            long span = viewportEnd - viewportStart;
            long dt   = (long)(dx / (right - left) * span);
            long newStart = viewportStart + dt;
            long newEnd   = viewportEnd   + dt;
            if (newStart < rangeStart) { newStart = rangeStart; newEnd = newStart + span; }
            if (newEnd   > rangeEnd)   { newEnd   = rangeEnd;   newStart = newEnd - span; }

            viewportStart = newStart;
            viewportEnd   = newEnd;
            invalidate();
            return true;
        }

        @Override
        public boolean onDoubleTap(MotionEvent e) {
            viewportStart = rangeStart;
            viewportEnd   = rangeEnd;
            invalidate();
            return true;
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (viewportEnd <= viewportStart) return;

        float width  = getWidth();
        float height = getHeight();
        float left   = graphLeft();
        float right  = graphRight();
        float top    = dp(4);
        float bottom = height - dp(14);

        long span = viewportEnd - viewportStart;
        long gridInterval  = pickGridInterval(span);
        long labelInterval = pickLabelInterval(span);

        // local-time offset for the visible range (DST-aware at the viewport mid)
        TimeZone tz = TimeZone.getDefault();
        long tzOffset = tz.getOffset(viewportStart + span / 2);

        // vertical (time) grid lines
        long firstGrid = ceilTo(viewportStart, gridInterval);
        for (long ts = firstGrid; ts <= viewportEnd; ts += gridInterval) {
            float x = mapTsToX(ts, left, right);
            canvas.drawLine(x, top, x, bottom, hourPaint);
        }

        // x-axis baseline
        canvas.drawLine(left, bottom, right, bottom, gridPaint);

        // 4 horizontal gridlines + rotated value annotations
        float colWidth  = dp(13);
        float colsStart = dp(8);
        int savedColor = labelPaint.getColor();
        for (float fraction : GRID_FRACTIONS) {
            float y = bottom - (bottom - top) * fraction;
            canvas.drawLine(left, y, right, y, gridPaint);
            for (int i = 0; i < seriesList.size(); i++) {
                Series s = seriesList.get(i);
                int value = (int)Math.round(s.yMin + (s.yMax - s.yMin) * fraction);
                labelPaint.setColor(s.color);
                drawRotatedLabel(canvas, String.valueOf(value), colsStart + i * colWidth, y);
            }
        }
        labelPaint.setColor(savedColor);

        // x-axis labels
        long firstLabel = ceilTo(viewportStart, labelInterval);
        boolean showMinutes = labelInterval < HOUR_MS;
        for (long ts = firstLabel; ts <= viewportEnd; ts += labelInterval) {
            String text = formatTime(ts, tzOffset, showMinutes);
            float x = mapTsToX(ts, left, right);
            float w = labelPaint.measureText(text);
            canvas.drawText(text, x - w / 2f, height - dp(2), labelPaint);
        }

        // shared 0 baseline label
        canvas.drawText("0", dp(2), bottom - dp(2), labelPaint);

        // dots
        for (Series s : seriesList) {
            dotPaint.setColor(s.color);
            double valueSpan = s.yMax - s.yMin;
            if (valueSpan <= 0) continue;
            for (Point p : s.points) {
                if (p.ts < viewportStart || p.ts > viewportEnd) continue;
                float x = mapTsToX(p.ts, left, right);
                float y = bottom - (float)((p.value - s.yMin) / valueSpan) * (bottom - top);
                canvas.drawCircle(x, y, dotRadius, dotPaint);
            }
        }
    }

    private float mapTsToX(long ts, float left, float right) {
        double frac = (double)(ts - viewportStart) / (double)(viewportEnd - viewportStart);
        return left + (float)(frac * (right - left));
    }

    private static long ceilTo(long ts, long step) {
        long r = ts % step;
        return r == 0 ? ts : ts + (step - r);
    }

    private static long pickGridInterval(long span) {
        if (span >= 12 * HOUR_MS) return HOUR_MS;
        if (span >=  6 * HOUR_MS) return 30 * MIN_MS;
        if (span >=  3 * HOUR_MS) return 15 * MIN_MS;
        if (span >=      HOUR_MS) return  5 * MIN_MS;
        if (span >= 30 * MIN_MS)  return  2 * MIN_MS;
        return MIN_MS;
    }

    private static long pickLabelInterval(long span) {
        if (span >= 12 * HOUR_MS) return 6 * HOUR_MS;
        if (span >=  6 * HOUR_MS) return 2 * HOUR_MS;
        if (span >=  3 * HOUR_MS) return     HOUR_MS;
        if (span >=      HOUR_MS) return 30 * MIN_MS;
        if (span >= 30 * MIN_MS)  return 10 * MIN_MS;
        if (span >= 15 * MIN_MS)  return  5 * MIN_MS;
        return 2 * MIN_MS;
    }

    private static String formatTime(long ts, long tzOffset, boolean showMinutes) {
        long local = ts + tzOffset;
        int hour   = (int)((local / HOUR_MS) % 24);
        int minute = (int)((local / MIN_MS) % 60);
        if (showMinutes) {
            return String.format("%02d:%02d", hour, minute);
        }
        return String.format("%02d", hour);
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
