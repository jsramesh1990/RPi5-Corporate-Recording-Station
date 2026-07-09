# Hardware Calibration Guide
## RPi5 Corporate Recording Station

## 📋 Overview

This guide covers the calibration procedures for all hardware components to ensure optimal performance and recording quality.

## 🎯 Calibration Sequence

### 1. UART Calibration

#### 1.1 Baud Rate Verification
```bash
# Test UART loopback (requires jumper wire)
# Connect GPIO Pin 8 (TX) to Pin 10 (RX)

# Send test data
echo "TEST" > /dev/ttyAMA0

# Should receive "TEST" back
```

#### 1.2 Timing Calibration
```bash
# Check baud rate accuracy
sudo screen /dev/ttyAMA0 115200

# Send a known string
# Verify no character corruption
```

### 2. Audio Calibration

#### 2.1 Input Level Calibration
```bash
# Test audio input
arecord -d 5 -f S16_LE -r 48000 -c 2 test.wav

# Play back
aplay test.wav

# Use audio meter to verify levels
alsamixer
# Adjust input gain for -6dB to -3dB peak
```

#### 2.2 Latency Testing
```bash
# Measure audio latency
# Connect audio output to input
# Record and analyze delay
ffmpeg -i test.wav -af "adelay=0|0" delayed.wav
```

### 3. Video Calibration

#### 3.1 Resolution and Framerate
```bash
# Test video capture with different settings
ffmpeg -f v4l2 -i /dev/video0 -t 10 test.mp4

# Verify settings
v4l2-ctl -d /dev/video0 --all

# Check supported formats
v4l2-ctl -d /dev/video0 --list-formats-ext
```

#### 3.2 Color Calibration
```bash
# Use color bars
v4l2-ctl -d /dev/video0 --set-ctrl=test_pattern=1

# Check color accuracy
# Adjust brightness/contrast
v4l2-ctl -d /dev/video0 --set-ctrl=brightness=128
v4l2-ctl -d /dev/video0 --set-ctrl=contrast=128
```

### 4. Storage Performance Calibration

#### 4.1 NVMe Benchmark
```bash
# Test sequential read/write
sudo dd if=/dev/zero of=/mnt/nvme/test bs=1M count=1024 conv=fdatasync
# Expected: >400 MB/s

# Test random I/O
sudo fio --filename=/mnt/nvme/test --size=1G --direct=1 --rw=randread --bs=4k --numjobs=4 --time_based --runtime=30
```

#### 4.2 Buffer Size Optimization
```bash
# Test different write buffer sizes
for size in 512 1024 2048 4096 8192; do
    echo "Testing $size KB buffer..."
    dd if=/dev/zero of=/mnt/nvme/test bs=${size}K count=1024 conv=fdatasync 2>&1 | grep MB/s
done
```

### 5. Temperature Calibration

#### 5.1 Idle Temperature Baseline
```bash
# Monitor temperature at idle
watch -n 1 "vcgencmd measure_temp"

# Record baseline after 30 minutes
# Should be < 45°C
```

#### 5.2 Load Temperature Test
```bash
# Run stress test
stress --cpu 4 --timeout 300

# Monitor temperature
# Should stay < 80°C
```

## 📊 Calibration Settings

### Recommended Settings

| Component | Setting | Value | Notes |
|-----------|---------|-------|-------|
| **UART** | Baud Rate | 115200 | 8N1 |
| **Audio** | Sample Rate | 48000 Hz | CD Quality |
| **Audio** | Channels | 2 | Stereo |
| **Video** | Resolution | 1920x1080 | Full HD |
| **Video** | Framerate | 30 fps | Standard |
| **Video** | Bitrate | 4 Mbps | Good quality |
| **NVMe** | Write Buffer | 4096 KB | Optimal |
| **NVMe** | I/O Scheduler | mq-deadline | For NVMe |

## 🔧 Calibration Tools

### Required Tools
```bash
# Install calibration tools
sudo apt install -y \
    v4l-utils \
    alsa-utils \
    fio \
    stress \
    iperf3 \
    evtest \
    i2c-tools
```

### Custom Calibration Script
```bash
#!/bin/bash
# calibration.sh - Complete system calibration

echo "Starting system calibration..."

# 1. Audio calibration
echo "Testing audio..."
arecord -d 3 -f S16_LE -r 48000 -c 2 test_audio.wav
if [ $? -eq 0 ]; then
    echo "✓ Audio pass"
else
    echo "✗ Audio fail"
fi

# 2. Video calibration
echo "Testing video..."
ffmpeg -f v4l2 -i /dev/video0 -t 1 -f null /dev/null 2>&1 | grep "video"
if [ $? -eq 0 ]; then
    echo "✓ Video pass"
else
    echo "✗ Video fail"
fi

# 3. Storage calibration
echo "Testing storage..."
dd if=/dev/zero of=/mnt/nvme/test bs=1M count=100 conv=fdatasync 2>&1 | grep MB/s
if [ $? -eq 0 ]; then
    echo "✓ Storage pass"
else
    echo "✗ Storage fail"
fi

# 4. Network calibration
echo "Testing network..."
iperf3 -c 8.8.8.8 -t 5
if [ $? -eq 0 ]; then
    echo "✓ Network pass"
else
    echo "✗ Network fail"
fi

echo "Calibration complete"
```

## 📈 Performance Verification

### Expected Performance Metrics

| Metric | Expected | Acceptable Range |
|--------|----------|------------------|
| **Write Speed (NVMe)** | 400 MB/s | 350-500 MB/s |
| **Read Speed (NVMe)** | 450 MB/s | 400-550 MB/s |
| **Audio Latency** | 20 ms | 10-50 ms |
| **Video Latency** | 100 ms | 50-200 ms |
| **CPU Temp (Idle)** | 35°C | 30-45°C |
| **CPU Temp (Load)** | 65°C | 50-80°C |
| **UART Error Rate** | 0% | <0.01% |

## 🔄 Recalibration Schedule

| Component | Frequency | When to Recalibrate |
|-----------|-----------|---------------------|
| **Audio** | Monthly | After hardware changes |
| **Video** | Monthly | After resolution changes |
| **Storage** | Quarterly | After firmware updates |
| **Network** | Quarterly | After network changes |
| **Temperature** | Weekly | Seasonal changes |

---

