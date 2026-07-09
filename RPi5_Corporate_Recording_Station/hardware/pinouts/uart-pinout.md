# UART Console Pinout
## Raspberry Pi 5 UART Configuration

## 📋 Overview

The Raspberry Pi 5 has multiple UART interfaces. The primary console can be accessed via either the dedicated 3-pin connector or GPIO pins 14/15.

## 🔌 UART Interfaces

### Interface Overview

| UART | Device | Type | Pins | Default |
|------|--------|------|------|---------|
| UART0 | /dev/ttyAMA0 | PL011 | GPIO 14/15 | Console |
| UART1 | /dev/ttyAMA10 | PL011 | Dedicated 3-pin | Console |
| UART2 | /dev/ttyAMA1 | PL011 | GPIO 0/1 | Auxiliary |
| UART3 | /dev/ttyAMA2 | PL011 | GPIO 4/5 | Auxiliary |
| UART4 | /dev/ttyAMA3 | PL011 | GPIO 8/9 | Auxiliary |
| UART5 | /dev/ttyAMA4 | PL011 | GPIO 12/13 | Auxiliary |

### 1. Dedicated 3-pin UART Connector (Recommended)

**Location**: Next to HDMI ports

| Pin | Signal | Color | Direction | Description |
|-----|--------|-------|-----------|-------------|
| 1 | TX | White | Output | Transmit data |
| 2 | RX | Green | Input | Receive data |
| 3 | GND | Black | - | Ground |

**Connection**:
```
Dedicated Connector → USB-to-UART Adapter
    Pin 1 (TX)     →     RX (White)
    Pin 2 (RX)     →     TX (Green)
    Pin 3 (GND)    →     GND (Black)
```

### 2. GPIO UART (Pins 14 & 15)

**Location**: 40-pin GPIO header

| Raspberry Pi Pin | GPIO | Signal | Direction |
|------------------|------|--------|-----------|
| Pin 8 | GPIO 14 | UART0 TX | Output |
| Pin 10 | GPIO 15 | UART0 RX | Input |
| Pin 6 | GND | Ground | - |

**Connection**:
```
GPIO Header → USB-to-UART Adapter
    Pin 8 (TX)   →     RX (White)
    Pin 10 (RX)  →     TX (Green)
    Pin 6 (GND)  →     GND (Black)
```

## 🔧 Configuration

### Enable UART Console
```bash
# Edit config.txt
sudo nano /boot/firmware/config.txt

# Add/Enable:
enable_uart=1

# For GPIO pins (14/15):
dtparam=uart0=on
dtparam=uart0_console

# For dedicated connector:
# (Comment out uart0 lines above)
# Use default behavior
```

### Boot Parameters
```bash
# Edit cmdline.txt
sudo nano /boot/firmware/cmdline.txt

# Add console parameter:
console=serial0,115200
# or
console=ttyAMA0,115200
# or
console=ttyAMA10,115200
```

## 📊 UART Specifications

### Default Settings
| Parameter | Value |
|-----------|-------|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| Flow Control | None |
| Voltage | 3.3V |

### Supported Baud Rates
```
9600
19200
38400
57600
115200  ← Default
230400
460800
921600
1000000
1500000
```

## 🛠️ Testing UART

### Loopback Test (requires jumper)
```bash
# Connect GPIO Pin 8 (TX) to Pin 10 (RX)

# Send test data
echo "TEST" > /dev/ttyAMA0

# Monitor for response
cat /dev/ttyAMA0
```

### Serial Terminal Connection
```bash
# Using screen
screen /dev/ttyUSB0 115200

# Using minicom
minicom -D /dev/ttyUSB0 -b 115200

# Using tio
tio -b 115200 /dev/ttyUSB0

# Using picocom
picocom -b 115200 /dev/ttyUSB0
```

## 🐛 Troubleshooting

### UART Not Working
```bash
# Check configuration
cat /boot/firmware/config.txt | grep uart

# Check device exists
ls -l /dev/ttyAMA*

# Check permissions
ls -la /dev/ttyAMA0

# Add user to dialout group
sudo usermod -a -G dialout $USER

# Check baud rate
stty -F /dev/ttyAMA0
```

### No Console Output
```bash
# Check boot config
sudo grep -r "console" /boot/firmware/

# Check kernel messages
dmesg | grep -i uart

# Check service
sudo systemctl status serial-getty@ttyAMA0
```

---

