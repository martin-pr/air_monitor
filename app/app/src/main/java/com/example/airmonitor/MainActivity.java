package com.example.airmonitor;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Switch;
import android.widget.TextView;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

public class MainActivity extends Activity {
    private static final int PERMISSION_REQUEST = 1;
    private static final long DAY_MS = 24 * 60 * 60 * 1000L;

    private HistoryGraphView graph;
    private TextView valueCo2;
    private TextView valueTemp;
    private TextView valueHumidity;
    private TextView valueBattery;
    private ListView logList;
    private View tabGraphContent;
    private View tabLogContent;
    private View tabSettingsContent;
    private View tabGraphButton;
    private View tabLogButton;
    private View tabSettingsButton;
    private TextView tabGraphLabel;
    private TextView tabLogLabel;
    private TextView tabSettingsLabel;

    private static final int TAB_GRAPH    = 0;
    private static final int TAB_LOG      = 1;
    private static final int TAB_SETTINGS = 2;

    private static final int TAB_COLOR_SELECTED   = 0xFF000000;
    private static final int TAB_COLOR_UNSELECTED = 0x99000000;

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
        valueCo2      = findViewById(R.id.value_co2);
        valueTemp     = findViewById(R.id.value_temp);
        valueHumidity = findViewById(R.id.value_humidity);
        valueBattery  = findViewById(R.id.value_battery);
        logList       = findViewById(R.id.log_list);

        ((TextView)findViewById(R.id.legend_co2)).setTextColor(COLOR_CO2);
        ((TextView)findViewById(R.id.legend_temp)).setTextColor(COLOR_TEMP);
        ((TextView)findViewById(R.id.legend_humidity)).setTextColor(COLOR_HUMIDITY);

        setupTabs();

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
        refreshValues();
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

    private void setupTabs() {
        tabGraphContent    = findViewById(R.id.tab_graph);
        tabLogContent      = findViewById(R.id.tab_log);
        tabSettingsContent = findViewById(R.id.tab_settings);
        tabGraphButton     = findViewById(R.id.tab_btn_graph);
        tabLogButton       = findViewById(R.id.tab_btn_log);
        tabSettingsButton  = findViewById(R.id.tab_btn_settings);
        tabGraphLabel      = findViewById(R.id.tab_btn_graph_label);
        tabLogLabel        = findViewById(R.id.tab_btn_log_label);
        tabSettingsLabel   = findViewById(R.id.tab_btn_settings_label);

        tabGraphButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { selectTab(TAB_GRAPH); }
        });
        tabLogButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { selectTab(TAB_LOG); }
        });
        tabSettingsButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { selectTab(TAB_SETTINGS); }
        });

        selectTab(TAB_GRAPH);
    }

    private void selectTab(int tab) {
        tabGraphContent.setVisibility(   tab == TAB_GRAPH    ? View.VISIBLE : View.GONE);
        tabLogContent.setVisibility(     tab == TAB_LOG      ? View.VISIBLE : View.GONE);
        tabSettingsContent.setVisibility(tab == TAB_SETTINGS ? View.VISIBLE : View.GONE);

        applyTabLabelStyle(tabGraphLabel,    tab == TAB_GRAPH);
        applyTabLabelStyle(tabLogLabel,      tab == TAB_LOG);
        applyTabLabelStyle(tabSettingsLabel, tab == TAB_SETTINGS);

        if (tab == TAB_GRAPH) {
            refreshGraphs();
            refreshValues();
        } else if (tab == TAB_LOG) {
            refreshLog();
        }
    }

    private static void applyTabLabelStyle(TextView label, boolean selected) {
        label.setTextColor(selected ? TAB_COLOR_SELECTED : TAB_COLOR_UNSELECTED);
        label.setTypeface(null, selected ? Typeface.BOLD : Typeface.NORMAL);
    }

    private void refreshLog() {
        List<HistoryStore.Entry> entries = HistoryStore.read(this);
        // Most recent first.
        Collections.reverse(entries);
        logList.setAdapter(new LogAdapter(this, entries, AppSettings.getTempOffset(this)));
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

    private void refreshValues() {
        JSONObject json = WidgetState.getLastReading(this);
        String placeholder = getString(R.string.value_placeholder);
        if (json == null) {
            valueCo2.setText(placeholder);
            valueTemp.setText(placeholder);
            valueHumidity.setText(placeholder);
            valueBattery.setText(placeholder);
            return;
        }

        Object co2 = json.opt("co2");
        valueCo2.setText(co2 == null ? placeholder : co2 + " ppm");

        Object temp = json.opt("temp");
        if (temp == null) {
            valueTemp.setText(placeholder);
        } else {
            double adjusted = ((Number)temp).doubleValue() + AppSettings.getTempOffset(this);
            valueTemp.setText(String.format(Locale.US, "%.1f°C", adjusted));
        }

        Object humidity = json.opt("humidity");
        valueHumidity.setText(humidity == null ? placeholder : humidity + "%");

        if (json.optBoolean("charging", false)) {
            valueBattery.setText("⚡");
        } else {
            Object battery = json.opt("battery");
            valueBattery.setText(battery == null ? placeholder : battery + "%");
        }
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
