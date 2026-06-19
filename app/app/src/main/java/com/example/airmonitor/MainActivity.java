package com.example.airmonitor;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.Switch;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;

public class MainActivity extends Activity {
    private static final int PERMISSION_REQUEST = 1;
    private static final long DAY_MS = 24 * 60 * 60 * 1000L;

    private HistoryGraphView graph;

    private static final int COLOR_CO2      = 0xFFD32F2F;  // red
    private static final int COLOR_TEMP     = 0xFF7B1FA2;  // purple
    private static final int COLOR_HUMIDITY = 0xFF1976D2;  // blue

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(R.layout.activity_main);
        ReadingNotification.createChannel(this);

        Switch darkSwitch = findViewById(R.id.dark_widget_switch);
        darkSwitch.setChecked(AppSettings.isDarkWidget(this));
        darkSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton view, boolean checked) {
                AppSettings.setDarkWidget(MainActivity.this, checked);
                WidgetState.renderWidgets(MainActivity.this);
            }
        });

        Switch notifSwitch = findViewById(R.id.notifications_switch);
        notifSwitch.setChecked(AppSettings.isNotificationsEnabled(this));
        notifSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton view, boolean checked) {
                AppSettings.setNotificationsEnabled(MainActivity.this, checked);
                WidgetState.syncNotification(MainActivity.this);
            }
        });

        setupTempOffsetControls();

        graph = findViewById(R.id.graph);
        ViewGroup.LayoutParams sectionParams = findViewById(R.id.graph_section).getLayoutParams();
        sectionParams.height = getResources().getDisplayMetrics().heightPixels / 2;
        findViewById(R.id.graph_section).setLayoutParams(sectionParams);

        ((TextView)findViewById(R.id.legend_co2)).setTextColor(COLOR_CO2);
        ((TextView)findViewById(R.id.legend_temp)).setTextColor(COLOR_TEMP);
        ((TextView)findViewById(R.id.legend_humidity)).setTextColor(COLOR_HUMIDITY);

        String[] missing = missingPermissions();
        if (missing.length > 0) {
            requestPermissions(missing, PERMISSION_REQUEST);
        } else {
            onPermissionsReady();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshGraphs();
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        onPermissionsReady();
    }

    private void onPermissionsReady() {
        BeaconScanReceiver.register(this);
        requestBatteryOptimizationExemption();
    }

    private void setupTempOffsetControls() {
        final EditText input  = findViewById(R.id.temp_offset_value);
        Button         minus  = findViewById(R.id.temp_offset_minus);
        Button         plus   = findViewById(R.id.temp_offset_plus);

        input.setText(formatOffset(AppSettings.getTempOffset(this)));

        // Use a flag to suppress the TextWatcher when we update the text from the +/- buttons.
        final boolean[] suppress = { false };

        input.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}
            @Override
            public void afterTextChanged(Editable s) {
                if (suppress[0]) return;
                try {
                    float value = Float.parseFloat(s.toString());
                    AppSettings.setTempOffset(MainActivity.this, value);
                    onTempOffsetChanged();
                } catch (NumberFormatException ignored) {
                    // partial / invalid text — wait for more typing
                }
            }
        });

        minus.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { adjustTempOffset(input, suppress, -0.1f); }
        });
        plus.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { adjustTempOffset(input, suppress, +0.1f); }
        });
    }

    private void adjustTempOffset(EditText input, boolean[] suppress, float step) {
        float current = AppSettings.getTempOffset(this);
        float next    = Math.round((current + step) * 10f) / 10f;
        AppSettings.setTempOffset(this, next);
        suppress[0] = true;
        input.setText(formatOffset(next));
        input.setSelection(input.getText().length());
        suppress[0] = false;
        onTempOffsetChanged();
    }

    private static String formatOffset(float offset) {
        return String.format(Locale.US, "%.1f", offset);
    }

    private void onTempOffsetChanged() {
        WidgetState.renderWidgets(this);
        WidgetState.syncNotification(this);
        refreshGraphs();
    }

    private void refreshGraphs() {
        Calendar cal = Calendar.getInstance();
        cal.set(Calendar.HOUR_OF_DAY, 0);
        cal.set(Calendar.MINUTE, 0);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 0);
        long startTs = cal.getTimeInMillis();
        long endTs   = startTs + DAY_MS;

        List<HistoryStore.Entry> entries = HistoryStore.read(this);
        float tempOffset = AppSettings.getTempOffset(this);
        List<HistoryGraphView.Point> co2Points      = new ArrayList<>();
        List<HistoryGraphView.Point> tempPoints     = new ArrayList<>();
        List<HistoryGraphView.Point> humidityPoints = new ArrayList<>();
        for (HistoryStore.Entry e : entries) {
            if (e.ts < startTs || e.ts >= endTs) continue;
            co2Points.add(new HistoryGraphView.Point(e.ts, e.co2));
            tempPoints.add(new HistoryGraphView.Point(e.ts, e.temp + tempOffset));
            humidityPoints.add(new HistoryGraphView.Point(e.ts, e.humidity));
        }

        List<HistoryGraphView.Series> series = new ArrayList<>();
        series.add(new HistoryGraphView.Series(co2Points,      0, 2000, COLOR_CO2));
        series.add(new HistoryGraphView.Series(tempPoints,     0,   40, COLOR_TEMP));
        series.add(new HistoryGraphView.Series(humidityPoints, 0,  100, COLOR_HUMIDITY));

        graph.setRange(startTs, endTs);
        graph.setSeries(series);
    }

    // Without this exemption, OEM battery managers (Samsung One UI in particular)
    // suspend the PendingIntent scan when the screen is off, defeating the whole
    // point of using PendingIntent-based scanning.
    private void requestBatteryOptimizationExemption() {
        PowerManager pm = (PowerManager)getSystemService(POWER_SERVICE);
        if (pm == null || pm.isIgnoringBatteryOptimizations(getPackageName())) {
            return;
        }
        Intent intent = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
        intent.setData(Uri.parse("package:" + getPackageName()));
        try {
            startActivity(intent);
        } catch (RuntimeException ignored) {
            // some OEMs don't expose the dialog — user has to find it in Settings manually
        }
    }

    private String[] missingPermissions() {
        List<String> permissions = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            addMissing(permissions, Manifest.permission.BLUETOOTH_SCAN);
            addMissing(permissions, Manifest.permission.BLUETOOTH_CONNECT);
        } else {
            addMissing(permissions, Manifest.permission.ACCESS_FINE_LOCATION);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            addMissing(permissions, Manifest.permission.POST_NOTIFICATIONS);
        }
        return permissions.toArray(new String[0]);
    }

    private void addMissing(List<String> permissions, String permission) {
        if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
            permissions.add(permission);
        }
    }
}
