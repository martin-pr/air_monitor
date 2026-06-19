package com.example.airmonitor;

import android.content.Context;
import android.content.SharedPreferences;

public class AppSettings {
    private static final String PREFS                  = "settings";
    private static final String KEY_DARK_WIDGET        = "dark_widget";
    private static final String KEY_NOTIFICATIONS      = "notifications";
    private static final String KEY_TEMP_OFFSET        = "temp_offset";
    private static final float  DEFAULT_TEMP_OFFSET    = 4.0f;

    public static boolean isDarkWidget(Context context) {
        return prefs(context).getBoolean(KEY_DARK_WIDGET, false);
    }

    public static void setDarkWidget(Context context, boolean enabled) {
        prefs(context).edit().putBoolean(KEY_DARK_WIDGET, enabled).apply();
    }

    public static boolean isNotificationsEnabled(Context context) {
        return prefs(context).getBoolean(KEY_NOTIFICATIONS, true);
    }

    public static void setNotificationsEnabled(Context context, boolean enabled) {
        prefs(context).edit().putBoolean(KEY_NOTIFICATIONS, enabled).apply();
    }

    public static float getTempOffset(Context context) {
        return prefs(context).getFloat(KEY_TEMP_OFFSET, DEFAULT_TEMP_OFFSET);
    }

    public static void setTempOffset(Context context, float offset) {
        prefs(context).edit().putFloat(KEY_TEMP_OFFSET, offset).apply();
    }

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
