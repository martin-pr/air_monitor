package com.example.airmonitor;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class LogAdapter extends BaseAdapter {
    private final Context     context;
    private final List<HistoryStore.Entry> entries;
    private final double      tempOffset;

    private static final SimpleDateFormat TIME_FORMAT = new SimpleDateFormat("HH:mm:ss", Locale.US);

    public LogAdapter(Context context, List<HistoryStore.Entry> entries, double tempOffset) {
        this.context = context;
        this.entries = entries;
        this.tempOffset = tempOffset;
    }

    @Override public int    getCount()              { return entries.size(); }
    @Override public Object getItem(int position)   { return entries.get(position); }
    @Override public long   getItemId(int position) { return entries.get(position).ts; }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            convertView = LayoutInflater.from(context).inflate(R.layout.log_row, parent, false);
        }
        HistoryStore.Entry e = entries.get(position);

        ((TextView)convertView.findViewById(R.id.col_time)).setText(TIME_FORMAT.format(new Date(e.ts)));
        ((TextView)convertView.findViewById(R.id.col_co2)).setText(String.valueOf(e.co2));
        ((TextView)convertView.findViewById(R.id.col_temp)).setText(
            String.format(Locale.US, "%.1f", e.temp + tempOffset)
        );
        ((TextView)convertView.findViewById(R.id.col_humidity)).setText(
            String.format(Locale.US, "%.0f", e.humidity)
        );
        ((TextView)convertView.findViewById(R.id.col_battery)).setText(
            e.charging ? "⚡" : (e.battery < 0 ? "--" : String.valueOf(e.battery))
        );
        ((TextView)convertView.findViewById(R.id.col_status)).setText(statusCode(e.status));
        ((TextView)convertView.findViewById(R.id.col_device)).setText(deviceTag(e.mac));

        return convertView;
    }

    // Last 2 bytes of the MAC (e.g. "82FA"). Compact enough for a narrow column,
    // unique enough to distinguish a handful of devices in the household.
    private static String deviceTag(String mac) {
        if (mac == null || mac.length() < 5) return "";
        String tail = mac.substring(mac.length() - 5);  // last "xx:xx"
        return tail.replace(":", "").toUpperCase(Locale.US);
    }

    private static String statusCode(int status) {
        switch (status) {
            case 1:  return "POW";  // ESP_RST_POWERON
            case 3:  return "SW";   // ESP_RST_SW
            case 4:  return "PNC";  // ESP_RST_PANIC
            case 5:  return "IWD";  // ESP_RST_INT_WDT
            case 6:  return "TWD";  // ESP_RST_TASK_WDT
            case 7:  return "WDT";  // ESP_RST_WDT
            case 8:  return "";     // ESP_RST_DEEPSLEEP — normal, hide
            case 9:  return "BRN";  // ESP_RST_BROWNOUT
            case 11: return "USB";
            case 12: return "JTG";
            default: return "?" + status;
        }
    }
}
