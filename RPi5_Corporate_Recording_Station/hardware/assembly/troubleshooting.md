# Troubleshooting Guide
## RPi5 Corporate Recording Station

## 📋 Overview

Comprehensive troubleshooting guide for all hardware and software issues.

## 🔍 Diagnostic Flowchart

```
Start: System Not Working
        |
        v
   [Power LED On?] ---No---> [Check Power Supply]
        |
       Yes
        |
        v
   [ACT LED Blinking?] ---No---> [Check SD Card/Boot]
        |
       Yes
        |
        v
   [UART Console?] ---No---> [Check UART Connection]
        |
       Yes
        |
        v
   [Recording Station Running?] ---No---> [Check Service]
        |
       Yes
        |
        v
   [Recording Works?] ---No---> [Check Peripherals]
        |
       Yes
        |
        v
   [Cloud Uploads?] ---No---> [Check Network]
        |
       Yes
        |
        v
   [All Working!] ---> [System Operational]
```

## 🚨 Common Issues

### 1. Power Issues

#### Symptom: No power, no LEDs
```bash
# Check power supply
# Official 27W USB-C power supply is required

# Check voltage
# Use multimeter on GPIO pins 2 and 6
# Should read 5.0V ± 0.25V

# Check if undervoltage
vcgencmd get_throttled
# Bit 0: Under-voltage detected
# Bit 16: Under-voltage has occurred
```

#### Solution
```
1. Use official power supply only
2. Check USB-C cable quality
3. Remove peripherals to test
4. Check for loose connections
```

### 2. UART Issues

#### Symptom: No console output
```bash
# Check UART device
ls -l /dev/ttyAMA*

# Check config
cat /boot/firmware/config.txt | grep uart

# Check baud rate
stty -F /dev/ttyAMA0

# Test loopback (jumper pins 8 and 10)
echo "TEST" > /dev/ttyAMA0
# Should see "TEST" if working
```

#### Solution
```
# Ensure enable_uart=1 in config.txt
# Check wiring (GND, TX, RX)
# Verify baud rate matches (115200)
# Try different serial terminal
```

### 3. NVMe Detection Issues

#### Symptom: NVMe not detected
```bash
# Check PCIe
lspci -v

# Check kernel messages
dmesg | grep nvme

# Check block devices
lsblk

# Check PCIe config
cat /boot/firmware/config.txt | grep pcie
```

#### Solution
```
# Add to config.txt:
dtoverlay=pcie-32bit-dma

# Check FPC cable connection
# Check power to NVMe
# Check NVMe SSD compatibility
```

### 4. HDMI Capture Issues

#### Symptom: No video input
```bash
# Check USB detection
lsusb

# Check V4L2 devices
ls -l /dev/video*

# Check driver
dmesg | grep video

# Test capture
ffmpeg -f v4l2 -i /dev/video0 -t 1 test.mp4
```

#### Solution
```
# Try different USB port
# Check HDMI source
# Check cable connections
# Test with different capture card
```

### 5. Audio Issues

#### Symptom: No audio input
```bash
# Check audio devices
arecord -l

# Test recording
arecord -d 5 -f S16_LE -r 48000 -c 2 test.wav

# Check ALSA
alsamixer

# Check audio levels
# Look for mic boost and capture levels
```

#### Solution
```
# Check USB connection
# Check mic gain levels
# Try different audio interface
# Check ALSA configuration
```

### 6. Cloud Upload Issues

#### Symptom: Upload fails
```bash
# Check network connectivity
ping -c 4 8.8.8.8

# Check cloud status
curl -X GET http://localhost:8080/api/v1/cloud/status

# Check logs
tail -f /var/log/recording-station/cloud.log
```

#### Solution
```
# Check internet connection
# Verify cloud credentials
# Check cloud service status
# Increase timeout settings
```

### 7. Recording Issues

#### Symptom: Recording fails
```bash
# Check disk space
df -h

# Check permissions
ls -la /var/lib/recording-station/

# Check recording logs
tail -f /var/log/recording-station/recording.log
```

#### Solution
```
# Free up disk space
# Check file permissions
# Verify codec support
# Check hardware encoding
```

## 🛠️ Diagnostic Tools

### Built-in Tools
```bash
# Hardware diagnostics
vcgencmd commands:
  measure_temp      # CPU temperature
  measure_clock     # Clock frequencies
  get_throttled     # Throttling status
  get_config        # Configuration

# System diagnostics
top                # Process monitoring
htop               # Enhanced monitoring
df -h              # Disk usage
free -h            # Memory usage
dmesg -w           # Kernel messages
journalctl -f      # System logs
```

### Advanced Tools
```bash
# Install advanced tools
sudo apt install -y \
    i2c-tools \
    spi-tools \
    evtest \
    gdb \
    strace \
    valgrind \
    wireshark \
    tcpdump
```

## 📊 Error Codes Reference

| Code | Description | Solution |
|------|-------------|----------|
| **E001** | Power Under-voltage | Use official PSU |
| **E002** | UART Timeout | Check connection |
| **E003** | NVMe Not Found | Check PCIe config |
| **E004** | Camera Not Found | Check CSI connection |
| **E005** | Audio Device Error | Check ALSA config |
| **E006** | Network Timeout | Check internet |
| **E007** | Disk Full | Free up space |
| **E008** | Permission Denied | Check ownership |
| **E009** | Encoding Failed | Check codec |
| **E010** | Cloud Authentication | Check credentials |

## 🐛 Bug Reporting

### Collecting System Information
```bash
#!/bin/bash
# collect-debug-info.sh

echo "Collecting system information..."

# System info
echo "=== SYSTEM INFO ==="
uname -a
cat /etc/os-release

# Hardware info
echo "=== HARDWARE INFO ==="
vcgencmd version
vcgencmd get_throttled
vcgencmd measure_temp

# Storage info
echo "=== STORAGE INFO ==="
lsblk
df -h

# UART info
echo "=== UART INFO ==="
ls -l /dev/ttyAMA*
stty -F /dev/ttyAMA0

# Video info
echo "=== VIDEO INFO ==="
ls -l /dev/video*
v4l2-ctl --list-devices

# Audio info
echo "=== AUDIO INFO ==="
arecord -l
aplay -l

# Cloud info
echo "=== CLOUD INFO ==="
curl -X GET http://localhost:8080/api/v1/cloud/status

# Logs
echo "=== LAST 100 LOG LINES ==="
tail -n 100 /var/log/recording-station/recording-station.log
```

### Submitting Bug Report
```
1. Run collect-debug-info.sh and save output
2. Describe the issue clearly
3. Include steps to reproduce
4. Note any recent changes
5. Attach debug info to issue
```

## 🆘 Emergency Recovery

### Boot Recovery
```bash
# If system won't boot:
1. Press and hold SHIFT key during boot
2. This enters recovery mode
3. Check boot partition
4. Restore backup config
```

### Factory Reset
```bash
# Reset to factory settings
sudo ./scripts/reset.sh

# This will:
# - Remove all recordings
# - Reset configuration
# - Reinstall services
# - Reboot system
```

### Emergency Boot
```bash
# Boot with minimal configuration
# Add to config.txt:
force_turbo=1
avoid_warnings=1

# This bypasses:
# - UART
# - HDMI
# - USB
# - Audio
```

