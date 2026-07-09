# Audio Input Hardware Guide
## RPi5 Corporate Recording Station

## 📋 Overview

This guide covers audio input hardware options for the Raspberry Pi 5 Corporate Recording Station. Quality audio capture is essential for professional recordings.

## 🎯 Recommended Solutions

### 1. Professional Grade (Recommended)

#### Behringer U-Phoria UM2
- **Price**: $50
- **Interface**: USB 2.0
- **Inputs**: 1x XLR/TRS combo
- **Outputs**: 1x 1/4" TRS, 1x RCA
- **Features**:
  - 48V Phantom power
  - MIDAS preamp
  - 24-bit/48kHz
- **Pros**: Professional quality, reliable
- **Cons**: 1 input only

**Specifications**:
```
Sample Rate: 48kHz
Bit Depth: 24-bit
Input: 1x Combo (XLR/1/4")
Output: 1x 1/4" TRS, 1x RCA
Phantom Power: +48V
USB: 2.0
Power: USB bus powered
```

#### Focusrite Scarlett Solo (3rd Gen)
- **Price**: $120
- **Interface**: USB 2.0
- **Inputs**: 1x XLR, 1x 1/4" TRS
- **Features**:
  - 48V Phantom power
  - Air mode
  - 24-bit/192kHz
  - Latency-free monitoring
- **Pros**: Premium quality, low latency
- **Cons**: Expensive

### 2. Budget Options

#### USB Audio Adapter
- **Price**: $10
- **Interface**: USB 2.0
- **Inputs**: 1x 3.5mm stereo
- **Features**:
  - 16-bit/48kHz
  - Plug-and-play
  - Compact
- **Pros**: Cheap, simple
- **Cons**: Lower quality

#### I2S Audio HAT (WM8960)
- **Price**: $25
- **Interface**: I2S (GPIO)
- **Inputs**: 2x 3.5mm
- **Features**:
  - 24-bit/96kHz
  - Onboard microphone
  - Speaker output
- **Pros**: Good quality, low latency
- **Cons**: Requires GPIO

### 3. Microphones

#### Dynamic (Recommended for vocals)
- Shure SM58: $100
- Sennheiser e835: $90
- Audio-Technica AT2010: $80

#### Condenser (Recommended for instruments)
- Audio-Technica AT2020: $100
- Rode NT1-A: $230
- AKG P220: $150

## 🔌 Connection Guide

### USB Audio Setup
```
[Microphone] --XLR Cable--> [USB Audio Interface] --USB Cable--> [Raspberry Pi 5]
```

### GPIO/I2S Audio Setup
```
[Microphone] --3.5mm Cable--> [I2S Audio HAT] --40-pin GPIO--> [Raspberry Pi 5]
```

### Multi-Channel Setup
```
[Microphone 1] --XLR--> [Audio Interface 1] --USB--> [RPi5 USB Hub]
[Microphone 2] --XLR--> [Audio Interface 2] --USB--> [RPi5 USB Hub]
[Microphone 3] --XLR--> [Audio Interface 3] --USB--> [RPi5 USB Hub]
```

## 📊 Supported Formats

### Audio Formats
| Format | Sample Rate | Bit Depth | Channels |
|--------|-------------|-----------|----------|
| PCM | 44.1kHz | 16-bit | 1-2 |
| PCM | 48kHz | 16-bit | 1-2 |
| PCM | 44.1kHz | 24-bit | 1-2 |
| PCM | 48kHz | 24-bit | 1-2 |
| PCM | 96kHz | 24-bit | 1-2 |

### Codec Support
| Codec | Quality | Use Case |
|-------|---------|----------|
| AAC | High | Podcasts, music |
| MP3 | Medium | General use |
| FLAC | Lossless | Archiving |
| WAV | Uncompressed | Professional |
| Opus | High | Voice, streaming |

## 🛠️ Driver Installation

### USB Audio (ALSA)
USB audio devices usually work out of the box:

```bash
# Check audio devices
arecord -l
aplay -l

# Check USB
lsusb

# Test recording
arecord -d 5 -f S16_LE -r 48000 -c 2 test.wav

# Test playback
aplay test.wav
```

### I2S Audio HAT
```bash
# Edit config
sudo nano /boot/firmware/config.txt

# Add:
dtoverlay=wm8960
dtoverlay=i2s

# Install drivers
sudo apt install -y raspberrypi-utils

# Configure
sudo alsactl init
```

## 🔧 Configuration

### ALSA Settings
```bash
# Set default device
sudo nano /etc/asound.conf

# Add:
defaults.ctl.card 0
defaults.pcm.card 0

# Mixer settings
alsamixer

# Set levels:
# - Master: 75%
# - Capture: 90%
# - Mic Boost: +20dB
```

### PulseAudio (Optional)
```bash
# Install PulseAudio
sudo apt install -y pulseaudio

# Configure
sudo nano /etc/pulse/default.pa

# Add:
load-module module-alsa-source device=hw:0,0
load-module module-alsa-sink device=hw:0,0
```

## 📊 Performance Tuning

### Buffer Settings
```bash
# ALSA buffer size
sudo nano /etc/asound.conf

# Add:
pcm.!default {
    type hw
    card 0
    buffer_size 1024
    period_size 256
}
```

### Latency Optimization
```bash
# Set real-time priority
sudo nano /etc/security/limits.conf

# Add:
@audio - rtprio 95
@audio - memlock unlimited

# Add user to audio group
sudo usermod -a -G audio $USER
```

## 🐛 Troubleshooting

### No Audio Device
```bash
# Check USB
lsusb

# Check ALSA
aplay -l
arecord -l

# Check dmesg
dmesg | grep -i audio

# Reload ALSA
sudo alsa force-reload
```

### Low Volume
```bash
# Check mixer
alsamixer

# Increase volume:
# - Capture: 90-100%
# - Mic Boost: +20-30dB
# - Master: 75-85%
```

### Audio Quality Issues
```bash
# Check sample rate
arecord -f S16_LE -r 48000 -c 2 -t wav test.wav

# Check for clipping
ffmpeg -i test.wav -af "volumedetect" -f null /dev/null

# Adjust gain
alsamixer
# Reduce if clipping detected
```

---

