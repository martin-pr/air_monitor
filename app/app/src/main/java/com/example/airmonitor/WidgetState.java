package com.example.airmonitor;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.widget.RemoteViews;

import org.json.JSONException;
import org.json.JSONObject;

import java.text.DateFormat;
import java.util.Date;

public class WidgetState {
    private static final String PLACEHOLDER = "--";

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
        HistoryStore.append(context, json);
        renderWidgets(context);
        syncNotification(context);
    }

    public static void syncNotification(Context context) {
        if (!AppSettings.isNotificationsEnabled(context)) {
            ReadingNotification.cancel(context);
            return;
        }
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String jsonStr = prefs.getString(KEY_JSON, null);
        if (jsonStr == null) return;
        try {
            ReadingNotification.show(context, new JSONObject(jsonStr));
        } catch (JSONException ignored) {}
    }

    public static void saveStatus(Context context, String status) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_STATUS, status).apply();
        renderWidgets(context);
    }

    public static JSONObject getLastReading(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String jsonStr = prefs.getString(KEY_JSON, null);
        if (jsonStr == null) return null;
        try {
            return new JSONObject(jsonStr);
        } catch (JSONException e) {
            return null;
        }
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

        String tempStr     = formatTemperature(context, json);
        String humidityStr = formatHumidity(json);
        String co2Str      = formatCo2(json);
        String batteryStr  = formatBattery(json);
        String footer      = footerText(status, time);

        boolean dark = AppSettings.isDarkWidget(context);
        int backgroundDrawable = dark ? R.drawable.widget_background_dark : R.drawable.widget_background_light;
        int primaryColor   = context.getColor(dark ? R.color.widget_text_primary_dark   : R.color.widget_text_primary_light);
        int secondaryColor = context.getColor(dark ? R.color.widget_text_secondary_dark : R.color.widget_text_secondary_light);

        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        ComponentName component = new ComponentName(context, AirWidgetProvider.class);
        int[] ids = manager.getAppWidgetIds(component);

        for (int id : ids) {
            RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.widget_air_monitor);
            views.setOnClickPendingIntent(R.id.widget_root, openAppIntent(context));
            views.setInt(R.id.widget_root, "setBackgroundResource", backgroundDrawable);
            views.setTextViewText(R.id.temp_value,     tempStr);
            views.setTextViewText(R.id.humidity_value, humidityStr);
            views.setTextViewText(R.id.co2_value,      co2Str);
            views.setTextViewText(R.id.battery_value,  batteryStr);
            views.setTextViewText(R.id.widget_footer,  footer);
            views.setTextColor(R.id.temp_value,     primaryColor);
            views.setTextColor(R.id.humidity_value, primaryColor);
            views.setTextColor(R.id.co2_value,      primaryColor);
            views.setTextColor(R.id.battery_value,  primaryColor);
            views.setTextColor(R.id.co2_label,      primaryColor);
            views.setTextColor(R.id.widget_footer,  secondaryColor);
            manager.updateAppWidget(id, views);
        }
    }

    private static String formatTemperature(Context context, JSONObject json) {
        if (json == null) return PLACEHOLDER + "°C";
        Object temp = json.opt("temp");
        if (temp == null) return PLACEHOLDER + "°C";
        double adjusted = ((Number)temp).doubleValue() + AppSettings.getTempOffset(context);
        return String.format("%.1f°C", adjusted);
    }

    private static String formatHumidity(JSONObject json) {
        if (json == null) return PLACEHOLDER + "% RH";
        Object humidity = json.opt("humidity");
        return humidity == null ? PLACEHOLDER + "% RH" : humidity + "% RH";
    }

    private static String formatCo2(JSONObject json) {
        if (json == null) return PLACEHOLDER;
        Object co2 = json.opt("co2");
        return co2 == null ? PLACEHOLDER : co2.toString();
    }

    private static String formatBattery(JSONObject json) {
        if (json == null) return PLACEHOLDER + "%";
        if (json != null && json.optBoolean("charging", false)) return "⚡";
        Object battery = json.opt("battery");
        return battery == null ? PLACEHOLDER + "%" : battery + "%";
    }

    private static PendingIntent openAppIntent(Context context) {
        Intent intent = new Intent(context, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return PendingIntent.getActivity(
            context, 0, intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
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
