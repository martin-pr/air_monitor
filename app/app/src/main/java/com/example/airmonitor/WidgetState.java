package com.example.airmonitor;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.view.View;
import android.widget.RemoteViews;

import org.json.JSONException;
import org.json.JSONObject;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.List;

public class WidgetState {
    private static final String PREFS         = "widget_state";
    private static final String KEY_JSON      = "last_json";
    private static final String KEY_STATUS    = "last_status";
    private static final String KEY_TIMESTAMP = "last_update_time";

    public static void saveReading(Context context, JSONObject json) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit()
            .putString(KEY_JSON, json.toString())
            .putString(KEY_TIMESTAMP, DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date()))
            .putString(KEY_STATUS, "")
            .apply();
        renderWidgets(context);
        ReadingNotification.show(context, json);
    }

    public static void saveStatus(Context context, String status) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_STATUS, status).apply();
        renderWidgets(context);
    }

    public static void renderWidgets(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String jsonStr = prefs.getString(KEY_JSON, null);
        String status  = prefs.getString(KEY_STATUS, "");
        String time    = prefs.getString(KEY_TIMESTAMP, null);

        JSONObject json = null;
        if (jsonStr != null) {
            try {
                json = new JSONObject(jsonStr);
            } catch (JSONException ignored) {}
        }

        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        ComponentName component = new ComponentName(context, AirWidgetProvider.class);
        int[] ids = manager.getAppWidgetIds(component);

        for (int id : ids) {
            RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.widget_air_monitor);
            views.setOnClickPendingIntent(R.id.widget_root, openAppIntent(context));

            views.setTextViewText(R.id.widget_footer, footerText(status, time));
            views.setViewVisibility(R.id.widget_footer, View.VISIBLE);
            views.removeAllViews(R.id.table_rows);

            if (json != null) {
                Iterator<String> keys = jsonKeys(json).iterator();
                while (keys.hasNext()) {
                    String key = keys.next();
                    RemoteViews row = new RemoteViews(context.getPackageName(), R.layout.widget_row);
                    row.setTextViewText(R.id.row_key, key);
                    row.setTextViewText(R.id.row_value, String.valueOf(json.opt(key)));
                    views.addView(R.id.table_rows, row);
                }
            }

            manager.updateAppWidget(id, views);
        }
    }

    private static PendingIntent openAppIntent(Context context) {
        Intent intent = new Intent(context, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return PendingIntent.getActivity(
            context, 0, intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
    }

    private static List<String> jsonKeys(JSONObject json) {
        List<String> keys = new ArrayList<>();
        Iterator<String> iterator = json.keys();
        while (iterator.hasNext()) {
            keys.add(iterator.next());
        }
        return keys;
    }

    private static String footerText(String status, String time) {
        StringBuilder text = new StringBuilder();
        if (status != null && !status.isEmpty()) {
            text.append(status);
        }
        if (time != null) {
            if (text.length() > 0) {
                text.append(" · ");
            }
            text.append("Last update: ").append(time);
        }
        return text.toString();
    }
}
