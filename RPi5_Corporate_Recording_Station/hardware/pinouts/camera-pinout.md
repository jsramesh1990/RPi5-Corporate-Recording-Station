# Camera Interface Pinout
## Raspberry Pi 5 CSI Connector (J4)

## 📋 Overview

The Raspberry Pi 5 features a 22-pin MIPI CSI-2 connector for camera modules. It supports both Raspberry Pi Camera Modules and third-party cameras.

## 🔌 CSI Connector Pinout

**Location**: Between Ethernet and HDMI ports

| Pin | Signal | Function | Description |
|-----|--------|----------|-------------|
| 1 | GND | Ground | Ground reference |
| 2 | D0P | Data Lane 0+ | MIPI CSI-2 Data 0 Positive |
| 3 | D0N | Data Lane 0- | MIPI CSI-2 Data 0 Negative |
| 4 | GND | Ground | Ground reference |
| 5 | D1P | Data Lane 1+ | MIPI CSI-2 Data 1 Positive |
| 6 | D1N | Data Lane 1- | MIPI CSI-2 Data 1 Negative |
| 7 | GND | Ground | Ground reference |
| 8 | CLKP | Clock Lane+ | MIPI CSI-2 Clock Positive |
| 9 | CLKN | Clock Lane- | MIPI CSI-2 Clock Negative |
| 10 | GND | Ground | Ground reference |
| 11 | D2P | Data Lane 2+ | MIPI CSI-2 Data 2 Positive |
| 12 | D2N | Data Lane 2- | MIPI CSI-2 Data 2 Negative |
| 13 | GND | Ground | Ground reference |
| 14 | D3P | Data Lane 3+ | MIPI CSI-2 Data 3 Positive |
| 15 | D3N | Data Lane 3- | MIPI CSI-2 Data 3 Negative |
| 16 | GND | Ground | Ground reference |
| 17 | SCL | I2C Clock | Camera I2C Clock (1.8V) |
| 18 | SDA | I2C Data | Camera I2C Data (1.8V) |
| 19 | ENABLE | Enable | Camera power enable |
| 20 | GND | Ground | Ground reference |
| 21 | VDD | Power | 3.3V Power (max 200mA) |
| 22 | GND | Ground | Ground reference |

## 📷 Compatible Cameras

### Official Raspberry Pi Cameras

| Model | Sensor | Resolution | Framerate | Features |
|-------|--------|------------|-----------|----------|
| Camera Module 3 | Sony IMX708 | 12.3MP | 4K @ 30fps | HDR, PDAF |
| Camera Module 3 Wide | Sony IMX708 | 12.3MP | 4K @ 30fps | Wide angle |
| Camera Module 2 | Sony IMX219 | 8MP | 1080p @ 30fps | - |
| Camera Module 1 | OmniVision OV5647 | 5MP | 1080p @ 30fps | - |
| High Quality Camera | Sony IMX477 | 12.3MP | 1080p @ 30fps | Interchangeable lens |

### Third-Party Compatible Cameras

| Model | Sensor | Resolution | Features |
|-------|--------|------------|----------|
| Arducam 16MP | IMX519 | 16MP | Autofocus |
| Waveshare 5MP | OV5640 | 5MP | - |
| Raspberry Pi Global Shutter | IMX296 | 3.1MP | Global shutter |

## 🔌 Connection Guide

### Connecting a Camera
```
1. Locate CSI connector (J4) on Pi
2. Carefully open the connector latch
3. Insert FPC cable:
   - Gold contacts facing away from Ethernet port
   - Ensure cable is fully inserted
4. Close the latch securely
5. Route cable through enclosure
6. Secure camera in desired position
```

### Camera Detection
```bash
# Check camera detection
vcgencmd get_camera

# Expected output:
# supported=1 detected=1

# Check I2C
i2cdetect -y 10  # Camera I2C bus
# Should show device at 0x1a (IMX219) or 0x1a (IMX708)
```

## 📊 Camera Specifications

### MIPI CSI-2 Specifications
| Parameter | Value |
|-----------|-------|
| Lanes | 4-lane |
| Speed | Up to 1.5 Gbps/lane |
| Resolution | Up to 4Kp60 |
| Protocol | MIPI CSI-2 v1.3 |
| Voltage | 1.8V (I2C), 3.3V (Power) |

### Supported Formats
| Format | Color Space | Bit Depth |
|--------|-------------|-----------|
| YUYV | YUV | 8-bit |
| RGB888 | RGB | 8-bit |
| RAW8 | RAW | 8-bit |
| RAW10 | RAW | 10-bit |
| RAW12 | RAW | 12-bit |

## 🛠️ Configuration

### Enable Camera
```bash
# Edit config.txt
sudo nano /boot/firmware/config.txt

# Add:
dtoverlay=vc4-kms-v3d
dtoverlay=imx708

# Or for specific camera:
dtoverlay=imx219
dtoverlay=ov5647
```

### Camera Control
```bash
# List camera controls
v4l2-ctl -d /dev/video0 --list-ctrls

# Set controls
v4l2-ctl -d /dev/video0 --set-ctrl=brightness=128
v4l2-ctl -d /dev/video0 --set-ctrl=contrast=128
v4l2-ctl -d /dev/video0 --set-ctrl=saturation=128

# Test capture
libcamera-still -o test.jpg
```

## 🐛 Troubleshooting

### Camera Not Detected
```bash
# Check physical connection
# Check FPC cable orientation

# Check config
vcgencmd get_camera

# Check dmesg
dmesg | grep -i camera

# Check I2C
i2cdetect -y 10
```

### Poor Image Quality
```bash
# Check focus
# Adjust lens focus ring

# Check resolution
v4l2-ctl -d /dev/video0 --get-fmt-video

# Check exposure
v4l2-ctl -d /dev/video0 --get-ctrl=exposure
```

---

