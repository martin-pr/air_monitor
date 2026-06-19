package com.example.airmonitor;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import org.json.JSONObject;

public class ReadingNotification {
    public static final  String NOTIFICATION_CHANNEL_ID = "air_monitor_reading";
    private static final int    NOTIFICATION_ID         = 1;

    public static void createChannel(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }
        NotificationChannel channel = new NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            context.getString(R.string.app_name),
            NotificationManager.IMPORTANCE_LOW  // silent, visible in shade and on lock screen
        );
        channel.setShowBadge(false);
        NotificationManager manager = context.getSystemService(NotificationManager.class);
        if (manager != null) {
            manager.createNotificationChannel(channel);
        }
    }

    public static void cancel(Context context) {
        NotificationManager manager = (NotificationManager)context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.cancel(NOTIFICATION_ID);
        }
    }

    public static void show(Context context, JSONObject json) {
        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
            ? new Notification.Builder(context, NOTIFICATION_CHANNEL_ID)
            : new Notification.Builder(context);

        Notification notification = builder
            .setSmallIcon(R.drawable.ic_stat_air)
            .setContentTitle(title(json))
            .setContentText(text(context, json))
            .setSubText(subText(json))
            .setContentIntent(openAppIntent(context))
            .setOngoing(true)        // user can't swipe it away
            .setOnlyAlertOnce(true)  // no sound/vibration on updates
            .setShowWhen(true)
            .setVisibility(Notification.VISIBILITY_PUBLIC)  // show full content on lock screen
            .build();

        NotificationManager manager = (NotificationManager)context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.notify(NOTIFICATION_ID, notification);
        }
    }

    private static String title(JSONObject json) {
        Object co2 = json.opt("co2");
        return co2 == null ? "Air Monitor" : "CO2 " + co2 + " ppm";
    }

    private static String text(Context context, JSONObject json) {
        StringBuilder text = new StringBuilder();
        Object temp = json.opt("temp");
        Object humidity = json.opt("humidity");
        if (temp != null) {
            double adjusted = ((Number)temp).doubleValue() + AppSettings.getTempOffset(context);
            text.append(String.format("%.1f", adjusted)).append("°C");
        }
        if (humidity != null) {
            if (text.length() > 0) text.append(" · ");
            text.append(humidity).append("% RH");
        }
        return text.toString();
    }

    private static String subText(JSONObject json) {
        if (json.optBoolean("charging", false)) return "Charging";
        Object battery = json.opt("battery");
        return battery == null ? null : "Battery " + battery + "%";
    }

    private static PendingIntent openAppIntent(Context context) {
        Intent intent = new Intent(context, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return PendingIntent.getActivity(
            context, 0, intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
    }
}
