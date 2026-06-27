package com.example.airmonitor;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

public class HistoryStore {
    private static final String FILE          = "history.json";
    private static final long   RETENTION_MS  = 24 * 60 * 60 * 1000L;
    // BLE advertises the same beacon ~50 times per wake cycle and the system
    // delivers many of those — collapse repeats inside this window into a
    // single history entry. Safe so long as the wake interval is > this.
    private static final long   DEDUPE_WINDOW_MS = 30 * 1000L;

    public static final class Entry {
        public final long    ts;
        public final int     co2;
        public final double  temp;
        public final double  humidity;
        public final int     battery;   // 0–100, or -1 when charging / unknown
        public final boolean charging;
        public final int     status;    // esp_reset_reason_t (8 = deep sleep wake = normal)
        public final String  mac;       // beacon source MAC, or null for older entries

        public Entry(long ts, int co2, double temp, double humidity, int battery, boolean charging, int status, String mac) {
            this.ts = ts;
            this.co2 = co2;
            this.temp = temp;
            this.humidity = humidity;
            this.battery = battery;
            this.charging = charging;
            this.status = status;
            this.mac = mac;
        }
    }

    public static void append(Context context, JSONObject reading) {
        List<Entry> entries = read(context);
        long now = System.currentTimeMillis();
        String mac = reading.optString("mac", null);

        // Per-MAC dedupe — drop only if the most recent entry from THIS same
        // device was within DEDUPE_WINDOW_MS. Walking backward from the tail
        // is fine because entries are time-ordered and the window is short
        // (so we stop after a handful of iterations).
        for (int i = entries.size() - 1; i >= 0; i--) {
            Entry e = entries.get(i);
            if (e.ts < now - DEDUPE_WINDOW_MS) break;
            boolean sameMac = (mac == null) ? (e.mac == null) : mac.equals(e.mac);
            if (sameMac) return;
        }

        entries.add(new Entry(
            now,
            reading.optInt("co2", 0),
            reading.optDouble("temp", 0.0),
            reading.optDouble("humidity", 0.0),
            reading.optInt("battery", -1),
            reading.optBoolean("charging", false),
            reading.optInt("status", 8),  // 8 = ESP_RST_DEEPSLEEP (normal)
            mac
        ));

        long cutoff = now - RETENTION_MS;
        int firstFresh = 0;
        while (firstFresh < entries.size() && entries.get(firstFresh).ts < cutoff) {
            firstFresh++;
        }
        if (firstFresh > 0) {
            entries = new ArrayList<>(entries.subList(firstFresh, entries.size()));
        }

        write(context, entries);
    }

    public static List<Entry> read(Context context) {
        File file = new File(context.getFilesDir(), FILE);
        if (!file.exists()) return new ArrayList<>();
        try {
            byte[] bytes = Files.readAllBytes(file.toPath());
            JSONArray arr = new JSONArray(new String(bytes, StandardCharsets.UTF_8));
            List<Entry> entries = new ArrayList<>(arr.length());
            for (int i = 0; i < arr.length(); i++) {
                JSONObject obj = arr.getJSONObject(i);
                entries.add(new Entry(
                    obj.getLong("ts"),
                    obj.getInt("co2"),
                    obj.getDouble("temp"),
                    obj.getDouble("humidity"),
                    obj.optInt("battery", -1),
                    obj.optBoolean("charging", false),
                    obj.optInt("status", 8),
                    obj.optString("mac", null)
                ));
            }
            return entries;
        } catch (Exception ignored) {
            return new ArrayList<>();
        }
    }

    private static void write(Context context, List<Entry> entries) {
        File file = new File(context.getFilesDir(), FILE);
        try {
            JSONArray arr = new JSONArray();
            for (Entry e : entries) {
                JSONObject obj = new JSONObject();
                obj.put("ts", e.ts);
                obj.put("co2", e.co2);
                obj.put("temp", e.temp);
                obj.put("humidity", e.humidity);
                obj.put("battery", e.battery);
                obj.put("charging", e.charging);
                obj.put("status", e.status);
                if (e.mac != null) obj.put("mac", e.mac);
                arr.put(obj);
            }
            try (FileWriter writer = new FileWriter(file)) {
                writer.write(arr.toString());
            }
        } catch (IOException | org.json.JSONException ignored) {}
    }
}
