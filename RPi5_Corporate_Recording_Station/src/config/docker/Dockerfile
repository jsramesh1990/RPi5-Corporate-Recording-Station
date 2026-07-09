# ============================================================
# Prometheus Alerts Configuration
# RPi5 Corporate Recording Station
# ============================================================

groups:
  - name: recording_station_alerts
    interval: 30s
    rules:
      # CPU alerts
      - alert: HighCPUUsage
        expr: 100 - (avg(rate(node_cpu_seconds_total{mode="idle"}[5m])) * 100) > 80
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High CPU usage on recording station"
          description: "CPU usage is above 80% for 5 minutes"

      # Memory alerts
      - alert: HighMemoryUsage
        expr: (node_memory_MemTotal_bytes - node_memory_MemAvailable_bytes) / node_memory_MemTotal_bytes * 100 > 85
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage on recording station"
          description: "Memory usage is above 85% for 5 minutes"

      # Disk alerts
      - alert: HighDiskUsage
        expr: (node_filesystem_avail_bytes{mountpoint="/var/lib/recording-station"} / node_filesystem_size_bytes{mountpoint="/var/lib/recording-station"}) * 100 < 10
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Low disk space on recording station"
          description: "Less than 10% disk space remaining"

      # Temperature alerts
      - alert: HighTemperature
        expr: node_hwmon_temp_celsius > 75
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "High temperature on recording station"
          description: "Temperature is above 75°C"

      # Service down alerts
      - alert: RecordingStationDown
        expr: up{job="recording-station"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Recording station is down"
          description: "Recording station service has been down for 1 minute"
