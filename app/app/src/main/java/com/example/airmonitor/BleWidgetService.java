package com.example.airmonitor;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.appwidget.AppWidgetManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
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
import android.os.ParcelUuid;
import android.view.View;
import android.widget.RemoteViews;

import org.json.JSONObject;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.UUID;

public class BleWidgetService extends Service {
    private static final UUID SERVICE_UUID = UUID.fromString("f59c6ce6-b894-4e87-9c5b-b347b72c7e93");
    private static final UUID CHARACTERISTIC_UUID = UUID.fromString("3d455d99-f31a-4826-bf25-7c5f23cedc49");
    private static final UUID CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private static final String CHANNEL_ID = "air_monitor_ble";
    private static final int BLE_MTU = 512;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private boolean scanning;

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
        closeGatt();
        super.onDestroy();
    }

    private void startScan() {
        if (!hasBlePermissions()) {
            setStatus(getString(R.string.permission_needed));
            return;
        }
        if (gatt != null || scanning) {
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
        filters.add(new ScanFilter.Builder().setServiceUuid(new ParcelUuid(SERVICE_UUID)).build());
        ScanSettings settings = new ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_POWER)
            .build();
        scanner.startScan(filters, settings, scanCallback);
        scanning = true;
    }

    private void stopScan() {
        if (scanner != null && scanning && hasBlePermissions()) {
            scanner.stopScan(scanCallback);
        }
        scanning = false;
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            stopScan();
            connect(result.getDevice());
        }
    };

    private void connect(BluetoothDevice device) {
        if (!hasBlePermissions()) {
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            gatt = device.connectGatt(this, false, gattCallback, BluetoothDevice.TRANSPORT_LE);
        } else {
            gatt = device.connectGatt(this, false, gattCallback);
        }
    }

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt g, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED && hasBlePermissions()) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    g.requestMtu(BLE_MTU);
                } else {
                    g.discoverServices();
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                closeGatt();
                setStatus(getString(R.string.not_connected));
                handler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        startScan();
                    }
                }, 3000);
            }
        }

        @Override
        public void onMtuChanged(BluetoothGatt g, int mtu, int status) {
            if (hasBlePermissions()) {
                g.discoverServices();
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt g, int status) {
            BluetoothGattService service = g.getService(SERVICE_UUID);
            BluetoothGattCharacteristic characteristic =
                service == null ? null : service.getCharacteristic(CHARACTERISTIC_UUID);
            if (characteristic == null || !hasBlePermissions()) {
                setStatus(getString(R.string.not_connected));
                return;
            }

            g.setCharacteristicNotification(characteristic, true);
            BluetoothGattDescriptor descriptor = characteristic.getDescriptor(CCCD_UUID);
            if (descriptor == null) {
                setStatus(getString(R.string.not_connected));
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                int result = g.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                if (result != BluetoothGatt.GATT_SUCCESS) {
                    setStatus(getString(R.string.not_connected));
                }
            } else {
                descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                if (!g.writeDescriptor(descriptor)) {
                    setStatus(getString(R.string.not_connected));
                }
            }
        }

        @Override
        public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor descriptor, int status) {
            if (!CCCD_UUID.equals(descriptor.getUuid()) || status != BluetoothGatt.GATT_SUCCESS) {
                setStatus(getString(R.string.not_connected));
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt g, BluetoothGattCharacteristic characteristic) {
            handleBytes(characteristic.getValue());
        }

        @Override
        public void onCharacteristicChanged(
            BluetoothGatt g,
            BluetoothGattCharacteristic characteristic,
            byte[] value
        ) {
            handleBytes(value);
        }
    };

    private void handleBytes(byte[] bytes) {
        String text = new String(bytes, StandardCharsets.UTF_8);
        int nul = text.indexOf('\0');
        if (nul >= 0) {
            text = text.substring(0, nul);
        }
        text = text.trim();

        try {
            JSONObject json = new JSONObject(text);
            setData(json, "");
        } catch (Exception error) {
            JSONObject raw = new JSONObject();
            try {
                raw.put("raw", text);
            } catch (Exception ignored) {
            }
            setData(raw, "invalid JSON");
        }
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

    private void closeGatt() {
        if (gatt != null && hasBlePermissions()) {
            gatt.close();
        }
        gatt = null;
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
        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        ComponentName component = new ComponentName(context, AirWidgetProvider.class);
        int[] ids = manager.getAppWidgetIds(component);

        for (int id : ids) {
            RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.widget_air_monitor);
            views.setOnClickPendingIntent(R.id.widget_root, openAppIntent(context));
            views.setTextViewText(R.id.widget_status, json == null ? status : "");
            views.setViewVisibility(R.id.widget_status, json == null ? View.VISIBLE : View.GONE);
            views.removeAllViews(R.id.table_rows);

            if (json != null) {
                Iterator<String> keys = json.keys();
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
            context,
            0,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
    }
}
