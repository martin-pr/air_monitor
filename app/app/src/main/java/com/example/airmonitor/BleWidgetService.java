package com.example.airmonitor;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.appwidget.AppWidgetManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.View;
import android.widget.RemoteViews;

import org.json.JSONObject;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.List;

public class BleWidgetService extends Service {
    private static final int BEACON_COMPANY_ID = 0x4D41;
    private static final int BEACON_VERSION = 1;
    private static final int BEACON_PAYLOAD_LEN = 7;
    private static final String CHANNEL_ID = "air_monitor_ble";
    private static JSONObject lastJson;
    private static String lastStatus;
    private static String lastUpdateTime;

    private static final long SCAN_RESTART_MS = 3 * 60 * 1000;  // restart scan every 3 min if still not connected

    private final Handler handler = new Handler(Looper.getMainLooper());
    private BluetoothLeScanner scanner;
    private boolean scanning;

    private final Runnable scanRestart = new Runnable() {
        @Override
        public void run() {
            stopScan();
            startScan();
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        startForeground(1, notification(getString(R.string.scanning)));
        startScan();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startScan();
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        stopScan();
        super.onDestroy();
    }

    private void startScan() {
        if (!hasBlePermissions()) {
            setStatus(getString(R.string.permission_needed));
            return;
        }
        if (scanning) {
            return;
        }

        BluetoothManager manager = (BluetoothManager)getSystemService(BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        scanner = adapter == null ? null : adapter.getBluetoothLeScanner();
        if (scanner == null) {
            setStatus(getString(R.string.not_connected));
            return;
        }

        setStatus(getString(R.string.scanning));
        List<ScanFilter> filters = new ArrayList<>();
        filters.add(new ScanFilter.Builder()
            .setManufacturerData(
                BEACON_COMPANY_ID,
                new byte[] { BEACON_VERSION },
                new byte[] { (byte)0xFF }
            )
            .build());
        ScanSettings settings = new ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setMatchMode(ScanSettings.MATCH_MODE_AGGRESSIVE)
            .setNumOfMatches(ScanSettings.MATCH_NUM_MAX_ADVERTISEMENT)
            .build();
        scanner.startScan(filters, settings, scanCallback);
        scanning = true;
        handler.removeCallbacks(scanRestart);
        handler.postDelayed(scanRestart, SCAN_RESTART_MS);
    }

    private void stopScan() {
        handler.removeCallbacks(scanRestart);
        if (scanner != null && scanning && hasBlePermissions()) {
            scanner.stopScan(scanCallback);
        }
        scanning = false;
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            handleBeacon(result);
        }

        @Override
        public void onScanFailed(int errorCode) {
            setStatus(getString(R.string.not_connected));
        }
    };

    private void handleBeacon(ScanResult result) {
        ScanRecord record = result.getScanRecord();
        byte[] payload = record == null ? null : record.getManufacturerSpecificData(BEACON_COMPANY_ID);
        if (payload == null || payload.length < BEACON_PAYLOAD_LEN) {
            return;
        }

        if ((payload[0] & 0xFF) != BEACON_VERSION) {
            setStatus("unsupported beacon");
            return;
        }

        try {
            int co2 = u16le(payload, 1);
            int tempCenti = s16le(payload, 3);
            int humidity = payload[5] & 0xFF;
            int battery = payload[6] & 0xFF;

            JSONObject json = new JSONObject();
            json.put("co2", co2);
            json.put("temp", tempCenti / 100.0);
            json.put("humidity", humidity);
            json.put("battery", battery);
            setData(json, "");
        } catch (Exception error) {
            setStatus(getString(R.string.not_connected));
        }
    }

    private static int u16le(byte[] data, int offset) {
        return (data[offset] & 0xFF) | ((data[offset + 1] & 0xFF) << 8);
    }

    private static int s16le(byte[] data, int offset) {
        int value = u16le(data, offset);
        return value >= 0x8000 ? value - 0x10000 : value;
    }

    private void setStatus(String status) {
        setData(null, status);
    }

    private void setData(JSONObject json, String status) {
        updateWidgets(this, json, status);
        NotificationManager manager = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.notify(1, notification(json == null ? status : jsonSummary(json)));
        }
    }

    private static String jsonSummary(JSONObject json) {
        StringBuilder summary = new StringBuilder();
        Iterator<String> keys = json.keys();
        while (keys.hasNext()) {
            String key = keys.next();
            if (summary.length() > 0) {
                summary.append(", ");
            }
            summary.append(key).append(": ").append(String.valueOf(json.opt(key)));
        }
        return summary.toString();
    }

    private boolean hasBlePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        }
        return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    private Notification notification(String text) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Air Monitor",
                NotificationManager.IMPORTANCE_LOW
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            manager.createNotificationChannel(channel);
        }

        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
            ? new Notification.Builder(this, CHANNEL_ID)
            : new Notification.Builder(this);
        return builder
            .setContentTitle(getString(R.string.app_name))
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_stat_air)
            .build();
    }

    public static void updateWidgets(Context context, JSONObject json, String status) {
        if (json != null) {
            lastJson = json;
            lastUpdateTime = DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date());
        }
        lastStatus = status;

        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        ComponentName component = new ComponentName(context, AirWidgetProvider.class);
        int[] ids = manager.getAppWidgetIds(component);

        for (int id : ids) {
            RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.widget_air_monitor);
            views.setOnClickPendingIntent(R.id.widget_root, openAppIntent(context));
            JSONObject visibleJson = lastJson;

            views.setTextViewText(R.id.widget_footer, footerText(status));
            views.setViewVisibility(R.id.widget_footer, View.VISIBLE);
            views.removeAllViews(R.id.table_rows);

            if (visibleJson != null) {
                List<String> keys = jsonKeys(visibleJson);
                Iterator<String> iterator = keys.iterator();
                while (iterator.hasNext()) {
                    String key = iterator.next();
                    RemoteViews row = new RemoteViews(context.getPackageName(), R.layout.widget_row);
                    row.setTextViewText(R.id.row_key, key);
                    row.setTextViewText(R.id.row_value, String.valueOf(visibleJson.opt(key)));
                    views.addView(R.id.table_rows, row);
                }
            }

            manager.updateAppWidget(id, views);
        }
    }

    public static void refreshWidgets(Context context) {
        updateWidgets(context, lastJson, lastStatus == null ? context.getString(R.string.not_connected) : lastStatus);
    }

    private static PendingIntent openAppIntent(Context context) {
        Intent intent = new Intent(context, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return PendingIntent.getActivity(
            context,
            0,
            intent,
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

    private static String footerText(String status) {
        StringBuilder text = new StringBuilder();
        if (status != null && !status.isEmpty()) {
            text.append(status);
        }
        if (lastUpdateTime != null) {
            if (text.length() > 0) {
                text.append(" · ");
            }
            text.append("Last update: ").append(lastUpdateTime);
        }
        return text.toString();
    }
}
