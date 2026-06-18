package com.example.airmonitor;

import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

public class AirWidgetProvider extends AppWidgetProvider {
    @Override
    public void onReceive(Context context, Intent intent) {
        super.onReceive(context, intent);
        if (Intent.ACTION_MY_PACKAGE_REPLACED.equals(intent.getAction())
            || Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            BeaconScanReceiver.register(context);
        }
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] widgetIds) {
        BeaconScanReceiver.register(context);
        WidgetState.renderWidgets(context);
    }

    @Override
    public void onEnabled(Context context) {
        BeaconScanReceiver.register(context);
    }

    @Override
    public void onAppWidgetOptionsChanged(
        Context context,
        AppWidgetManager manager,
        int widgetId,
        Bundle newOptions
    ) {
        WidgetState.renderWidgets(context);
    }
}
