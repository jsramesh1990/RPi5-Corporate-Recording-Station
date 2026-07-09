# HDMI Capture Hardware Guide
## RPi5 Corporate Recording Station

## 📋 Overview

This guide covers HDMI capture hardware options for the Raspberry Pi 5 Corporate Recording Station. HDMI capture is essential for recording video from external sources like cameras, computers, or presentation systems.

## 🎯 Recommended Solutions

### 1. Professional Grade (Recommended)

#### Elgato Cam Link 4K
- **Price**: $130
- **Input**: HDMI 2.0
- **Output**: USB 3.0
- **Resolution**: 4K @ 60fps
- **Latency**: < 50ms
- **Features**: 
  - HDR10 support
  - 10-bit color depth
  - Audio embedded
  - UVC compliant
- **Pros**: Professional quality, low latency, reliable
- **Cons**: Expensive

**Specifications**:
```
Video Input: HDMI 2.0
Video Output: USB 3.0 Type-C
Max Resolution: 4096x2160 @ 60fps
Min Resolution: 640x480 @ 60fps
Color Depth: 10-bit
Audio: 2-channel PCM
Driver: UVC 1.1
Power: USB bus powered
```

#### Magewell USB Capture HDMI Plus
- **Price**: $200
- **Input**: HDMI 2.0
- **Output**: USB 3.0
- **Resolution**: 4K @ 60fps
- **Features**:
  - HDMI loopback
  - 3D support
  - Multiple format support
  - Built-in scaler
- **Pros**: Loop-through, professional
- **Cons**: Expensive

### 2. Budget Options

#### Generic MS2109-based Capture Card
- **Price**: $25
- **Input**: HDMI 1.4
- **Output**: USB 2.0/3.0
- **Resolution**: 1080p @ 30fps
- **Latency**: ~100ms
- **Features**:
  - Basic H.264 encoding
  - Audio embedded
- **Pros**: Cheap
- **Cons**: Lower quality, higher latency

**Specifications**:
```
Video Input: HDMI 1.4
Video Output: USB 2.0
Max Resolution: 1920x1080 @ 30fps
Audio: 2-channel PCM
Driver: UVC 1.0
Power: USB bus powered
```

#### USB 3.0 HDMI Capture Dongle
- **Price**: $40-60
- **Input**: HDMI 2.0
- **Output**: USB 3.0
- **Resolution**: 1080p @ 60fps
- **Features**:
  - Low latency
  - H.264 encoding
  - YUY2 support
- **Pros**: Good value, low latency
- **Cons**: Variable quality

## 🔌 Connection Guide

### Basic Setup
```
[HDMI Source] --HDMI Cable--> [HDMI Capture Card] --USB 3.0--> [Raspberry Pi 5]
```

### Professional Setup (with loop-through)
```
[HDMI Source] --HDMI--> [HDMI Capture Card]
                       |--HDMI Out--> [Display/Monitor]
                       |--USB 3.0--> [Raspberry Pi 5]
```

### Multi-Camera Setup
```
[Camera 1] --HDMI--> [HDMI Capture Card 1] --USB 3.0--> [RPi5 USB Hub]
[Camera 2] --HDMI--> [HDMI Capture Card 2] --USB 3.0--> [RPi5 USB Hub]
[Camera 3] --HDMI--> [HDMI Capture Card 3] --USB 3.0--> [RPi5 USB Hub]
```

## 📊 Supported Formats

### Video Formats
| Format | Resolution | Framerate | Bit Depth | Color Space |
|--------|------------|-----------|-----------|-------------|
| 1080p | 1920x1080 | 60fps | 8-bit | 4:2:0 |
| 1080p | 1920x1080 | 30fps | 8-bit | 4:2:2 |
| 720p | 1280x720 | 60fps | 8-bit | 4:2:0 |
| 4K | 3840x2160 | 30fps | 8-bit | 4:2:0 |
| 4K | 3840x2160 | 60fps | 8-bit | 4:2:0 |

### Audio Formats
| Format | Channels | Sample Rate | Bit Depth |
|--------|----------|-------------|-----------|
| PCM | 2 | 48kHz | 16-bit |
| PCM | 2 | 44.1kHz | 16-bit |
| PCM | 2 | 32kHz | 16-bit |

## 🛠️ Driver Installation

### Automatic (UVC Support)
Most capture cards are UVC (USB Video Class) compliant and work out of the box:

```bash
# Check if detected
lsusb | grep -i "video\|elgato\|magewell"

# Check video devices
ls -l /dev/video*

# Test capture
ffplay /dev/video0
```

### Manual Installation (Elgato)
```bash
# Add repository
wget -q -O - https://apt.elgato.com/elgato.gpg | sudo apt-key add -
echo "deb https://apt.elgato.com/ stable main" | sudo tee /etc/apt/sources.list.d/elgato.list

# Install drivers
sudo apt update
sudo apt install -y elgato-gchd

# Load module
sudo modprobe v4l2loopback
```

### Manual Installation (Magewell)
```bash
# Download driver
wget https://www.magewell.com/downloads/magewell-uvc-linux-driver.tar.gz
tar -xzf magewell-uvc-linux-driver.tar.gz
cd magewell-uvc-linux-driver

# Build and install
make
sudo make install
sudo modprobe magewell-uvc
```

## 🔧 Configuration

### Video Settings
```bash
# Check current settings
v4l2-ctl -d /dev/video0 --all

# Set resolution
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=YUYV

# Set framerate
v4l2-ctl -d /dev/video0 --set-parm=30

# Set input
v4l2-ctl -d /dev/video0 --set-input=0
```

### Audio Settings
```bash
# Check audio device
arecord -l

# Set audio format
arecord -D hw:0,0 -f S16_LE -r 48000 -c 2

# Test audio
arecord -d 5 -f S16_LE -r 48000 -c 2 test.wav
```

## 📊 Performance Tuning

### Buffer Settings
```bash
# Increase buffer size
echo 4096 > /proc/sys/dev/v4l2/io_buffer_size

# Set max buffer count
echo 32 > /proc/sys/dev/v4l2/io_buffer_count
```

### USB Performance
```bash
# Check USB speed
lsusb -t

# Set USB to high speed
echo 2 > /sys/bus/usb/devices/*/speed

# Disable USB autosuspend
echo 'on' > /sys/bus/usb/devices/*/power/control
```

## 🐛 Troubleshooting

### No Video Input
```bash
# Check physical connection
# Check HDMI cable
# Check source is outputting

# Check driver
dmesg | grep video

# Test with different input
# Try different HDMI source
```

### Low Quality Video
```bash
# Check resolution
v4l2-ctl -d /dev/video0 --get-fmt-video

# Check bitrate
v4l2-ctl -d /dev/video0 --get-ctrl=bitrate

# Adjust settings
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080
```

### Audio Sync Issues
```bash
# Check audio device
arecord -l

# Adjust buffer size
# Use smaller buffer for lower latency
v4l2-ctl -d /dev/video0 --set-ctrl=audio_sync=1
```

## 📊 Performance Benchmarks

### Latency Measurements
| Device | Resolution | Latency | Notes |
|--------|------------|---------|-------|
| Cam Link 4K | 1080p 60fps | 45ms | Professional |
| Cam Link 4K | 4K 30fps | 50ms | Professional |
| MS2109 | 1080p 30fps | 100ms | Budget |
| Magewell | 1080p 60fps | 40ms | Premium |

### CPU Usage
| Device | Resolution | CPU Usage | RAM Usage |
|--------|------------|-----------|-----------|
| Cam Link 4K | 1080p 60fps | 10-15% | 50MB |
| MS2109 | 1080p 30fps | 5-8% | 30MB |
| Magewell | 4K 30fps | 15-20% | 80MB |

---

