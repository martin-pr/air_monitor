package com.example.airmonitor;

import android.Manifest;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

public class BeaconScanReceiver extends BroadcastReceiver {
    private static final String ACTION_SCAN_RESULT = "com.example.airmonitor.BEACON_SCAN";

    private static final int BEACON_COMPANY_ID  = 0x4D41;  // 'AM'
    private static final int BEACON_VERSION     = 1;
    private static final int BEACON_PAYLOAD_LEN = 8;

    public static void register(Context context) {
        if (!hasBlePermissions(context)) {
            WidgetState.saveStatus(context, context.getString(R.string.permission_needed));
            return;
        }

        BluetoothManager manager  = (BluetoothManager)context.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter  = manager == null ? null : manager.getAdapter();
        BluetoothLeScanner scanner = adapter == null ? null : adapter.getBluetoothLeScanner();
        if (scanner == null) {
            WidgetState.saveStatus(context, context.getString(R.string.not_connected));
            return;
        }

        List<ScanFilter> filters = new ArrayList<>();
        filters.add(new ScanFilter.Builder()
            .setManufacturerData(
                BEACON_COMPANY_ID,
                new byte[] { BEACON_VERSION },
                new byte[] { (byte)0xFF }
            )
            .build());

        // SCAN_MODE_LOW_POWER: the system handles batching and timing; we don't need
        // LOW_LATENCY because the sensor only advertises every few minutes anyway.
        ScanSettings settings = new ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_POWER)
            .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
            .build();

        try {
            scanner.startScan(filters, settings, callbackIntent(context));
            WidgetState.saveStatus(context, context.getString(R.string.scanning));
        } catch (SecurityException error) {
            WidgetState.saveStatus(context, context.getString(R.string.permission_needed));
        }
    }

    private static PendingIntent callbackIntent(Context context) {
        Intent intent = new Intent(context, BeaconScanReceiver.class);
        intent.setAction(ACTION_SCAN_RESULT);
        return PendingIntent.getBroadcast(
            context, 0, intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE
        );
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        int errorCode = intent.getIntExtra(BluetoothLeScanner.EXTRA_ERROR_CODE, -1);
        if (errorCode != -1) {
            WidgetState.saveStatus(context, context.getString(R.string.not_connected));
            return;
        }

        ArrayList<ScanResult> results = intent.getParcelableArrayListExtra(
            BluetoothLeScanner.EXTRA_LIST_SCAN_RESULT
        );
        if (results == null || results.isEmpty()) {
            return;
        }

        // A batch can contain results from multiple "Air Monitor" devices (e.g.
        // old + new firmware in range simultaneously) — and from older firmware
        // whose payload doesn't parse against the current layout. Walk the batch
        // and keep the most recent successfully-parsed result; if any parse
        // succeeds, that one wins.
        JSONObject bestJson = null;
        for (ScanResult result : results) {
            JSONObject json = parseBeacon(result);
            if (json != null) {
                bestJson = json;
            }
        }

        if (bestJson != null) {
            WidgetState.saveReading(context, bestJson);
        }
    }

    private static JSONObject parseBeacon(ScanResult result) {
        ScanRecord record = result.getScanRecord();
        byte[] payload = record == null ? null : record.getManufacturerSpecificData(BEACON_COMPANY_ID);
        if (payload == null || payload.length < BEACON_PAYLOAD_LEN) {
            return null;
        }

        if ((payload[0] & 0xFF) != BEACON_VERSION) {
            return null;
        }

        try {
            int status     = payload[1] & 0xFF;
            int co2        = u16le(payload, 2);
            int tempCenti  = s16le(payload, 4);
            int humidity   = payload[6] & 0xFF;
            int batteryRaw = payload[7] & 0xFF;
            // 0xFF is the "charging" sentinel — battery reading is unreliable
            // during a charge cycle, so the firmware sends this in lieu of a %.
            boolean charging = batteryRaw == 0xFF;

            JSONObject json = new JSONObject();
            json.put("status", status);
            json.put("co2", co2);
            json.put("temp", tempCenti / 100.0);
            json.put("humidity", humidity);
            if (!charging) json.put("battery", batteryRaw);
            json.put("charging", charging);
            if (result.getDevice() != null) {
                json.put("mac", result.getDevice().getAddress());
            }
            return json;
        } catch (Exception ignored) {
            return null;
        }
    }

    private static int u16le(byte[] data, int offset) {
        return (data[offset] & 0xFF) | ((data[offset + 1] & 0xFF) << 8);
    }

    private static int s16le(byte[] data, int offset) {
        int value = u16le(data, offset);
        return value >= 0x8000 ? value - 0x10000 : value;
    }

    private static boolean hasBlePermissions(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
                && context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        }
        return context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }
}
