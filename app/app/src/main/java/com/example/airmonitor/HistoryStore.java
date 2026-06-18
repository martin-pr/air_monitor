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

    public static final class Entry {
        public final long   ts;
        public final int    co2;
        public final double temp;
        public final double humidity;

        public Entry(long ts, int co2, double temp, double humidity) {
            this.ts = ts;
            this.co2 = co2;
            this.temp = temp;
            this.humidity = humidity;
        }
    }

    public static void append(Context context, JSONObject reading) {
        List<Entry> entries = read(context);
        long now = System.currentTimeMillis();
        entries.add(new Entry(
            now,
            reading.optInt("co2", 0),
            reading.optDouble("temp", 0.0),
            reading.optDouble("humidity", 0.0)
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
                    obj.getDouble("humidity")
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
                arr.put(obj);
            }
            try (FileWriter writer = new FileWriter(file)) {
                writer.write(arr.toString());
            }
        } catch (IOException | org.json.JSONException ignored) {}
    }
}
