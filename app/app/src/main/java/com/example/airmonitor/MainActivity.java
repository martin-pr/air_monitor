package com.example.airmonitor;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends Activity {
    private static final int PERMISSION_REQUEST = 1;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        TextView text = new TextView(this);
        text.setPadding(32, 32, 32, 32);
        text.setText("Air Monitor\n\nGrant Bluetooth permissions, then add the widget.");
        setContentView(text);

        String[] missing = missingPermissions();
        if (missing.length > 0) {
            requestPermissions(missing, PERMISSION_REQUEST);
        } else {
            BeaconScanReceiver.register(this);
        }
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        BeaconScanReceiver.register(this);
    }

    private String[] missingPermissions() {
        List<String> permissions = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            addMissing(permissions, Manifest.permission.BLUETOOTH_SCAN);
            addMissing(permissions, Manifest.permission.BLUETOOTH_CONNECT);
        } else {
            addMissing(permissions, Manifest.permission.ACCESS_FINE_LOCATION);
        }
        return permissions.toArray(new String[0]);
    }

    private void addMissing(List<String> permissions, String permission) {
        if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
            permissions.add(permission);
        }
    }
}
