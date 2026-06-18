package com.example.airmonitor;

import android.content.Context;
import android.content.SharedPreferences;

public class AppSettings {
    private static final String PREFS                  = "settings";
    private static final String KEY_DARK_WIDGET        = "dark_widget";
    private static final String KEY_NOTIFICATIONS      = "notifications";

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

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
