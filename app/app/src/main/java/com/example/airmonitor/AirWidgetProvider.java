package com.example.airmonitor;

import android.Manifest;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

public class AirWidgetProvider extends AppWidgetProvider {
    @Override
    public void onReceive(Context context, Intent intent) {
        super.onReceive(context, intent);
        if (Intent.ACTION_MY_PACKAGE_REPLACED.equals(intent.getAction())
            || Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            startIfAllowed(context);
        }
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] widgetIds) {
        startIfAllowed(context);
    }

    @Override
    public void onEnabled(Context context) {
        startIfAllowed(context);
    }

    private static void startIfAllowed(Context context) {
        if (!hasBlePermissions(context)) {
            BleWidgetService.updateWidgets(context, null, context.getString(R.string.permission_needed));
            return;
        }

        BleWidgetService.updateWidgets(context, null, context.getString(R.string.not_connected));
        startService(context);
    }

    private static void startService(Context context) {
        Intent intent = new Intent(context, BleWidgetService.class);
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent);
            } else {
                context.startService(intent);
            }
        } catch (RuntimeException error) {
            BleWidgetService.updateWidgets(context, null, context.getString(R.string.open_app_to_connect));
        }
    }

    private static boolean hasBlePermissions(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
                && context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        }
        return context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }
}
