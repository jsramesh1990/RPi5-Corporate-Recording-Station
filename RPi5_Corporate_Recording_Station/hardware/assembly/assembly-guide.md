# RPi5 Corporate Recording Station - Assembly Guide

## 📋 Bill of Materials (BOM)

| Component | Quantity | Part Number | Notes | Approx Cost |
|-----------|----------|-------------|-------|-------------|
| **Raspberry Pi 5** | 1 | RPI5-MB-4GB/8GB | Main board | $80-120 |
| **USB-C Power Supply** | 1 | RPI-PSU-27W | 27W Official PSU | $15 |
| **MicroSD Card** | 1 | Any Class 10/U3 | 32GB+ capacity | $15 |
| **NVMe SSD Base** | 1 | Pimoroni NVMe Base | PCIe adapter | $25 |
| **NVMe SSD** | 1 | Any M.2 2230/2242 | 256GB+ storage | $40-100 |
| **HDMI Capture Card** | 1 | Elgato Cam Link 4K | Video input | $130 |
| **USB Audio Interface** | 1 | Behringer U-Phoria UM2 | Audio input | $50 |
| **Camera Module 3** | 1 | RPI-CAM-MOD3 | Optional video | $25 |
| **UART to USB Adapter** | 1 | CP2102/FTDI | Debug console | $10 |
| **Active Cooler** | 1 | Raspberry Pi Active Cooler | Thermal mgmt | $10 |
| **Enclosure** | 1 | 3D Printed | Protection | $20 |
| **Cables** | Various | - | HDMI, USB, etc. | $30 |
| **GPIO Breakout** | 1 | GPIO Extension | Optional | $10 |

**Total Estimated Cost**: $400-600

## 🛠️ Tools Required

### Essential Tools
- Phillips #0 and #1 screwdrivers
- Small tweezers
- Anti-static wrist strap
- Cable ties
- Multimeter (for testing)

### Optional Tools
- 3D Printer (for enclosure)
- Soldering iron
- USB Power meter
- Thermal paste

## 🔧 Step-by-Step Assembly

### Step 1: Prepare the Raspberry Pi 5

#### 1.1 Install Active Cooler
```
1. Locate the 4 mounting holes on the Pi board
2. Apply thermal paste (if not pre-applied)
3. Align the cooler with the mounting holes
4. Press firmly to ensure good thermal contact
5. Screw in the 4 mounting screws (cross-pattern)
6. Connect the fan to the 4-pin header (J7)
7. Fan should be on top of the CPU
```

#### 1.2 Install NVMe Base
```
1. Locate the PCIe connector (J7) on the Pi
2. Connect the FPC ribbon cable to J7
   - Ensure correct orientation (gold contacts facing down)
3. Mount the NVMe Base board on GPIO header
   - Align with pins 1-40
   - Press firmly but gently
4. Install NVMe SSD into M.2 slot
   - Insert at 30-degree angle
   - Press down and secure with screw
5. Secure the board with standoffs if provided
```

### Step 2: Mount in Enclosure

```
1. Prepare enclosure:
   - Remove any burrs from 3D printed parts
   - Install brass inserts if using
2. Install motherboard:
   - Use M2.5 standoffs (4 positions)
   - Secure with M2.5 screws
   - Do not overtighten
3. Install NVMe Base:
   - Use M2 standoffs (4 positions)
   - Secure with M2 screws
4. Install active cooler:
   - Ensure fan has proper clearance
   - Leave at least 5mm for airflow
5. Route cables:
   - Use cable ties to manage wires
   - Keep cables away from fan
   - Leave access to GPIO header
```

### Step 3: Connect Peripherals

#### 3.1 UART Console Connection

**Option A: Dedicated 3-pin Connector (Recommended)**

| 3-pin Connector | USB-to-UART Adapter |
|-----------------|---------------------|
| Pin 1 (TX) | RX (White) |
| Pin 2 (RX) | TX (Green) |
| Pin 3 (GND) | GND (Black) |

**Option B: GPIO Pins (14 & 15)**

| Raspberry Pi 5 | USB-to-UART Adapter |
|----------------|---------------------|
| Pin 6 (GND) | GND (Black) |
| Pin 8 (TX) | RX (White) |
| Pin 10 (RX) | TX (Green) |

#### 3.2 HDMI Capture Connection

```
[HDMI Source] --HDMI Cable--> [HDMI Capture Card]
                                    |
                                    v
                          [USB 3.0 Cable]
                                    |
                                    v
                        [RPi5 USB 3.0 Port]
```

#### 3.3 Audio Interface Connection

```
[Microphone] --XLR Cable--> [Audio Interface]
                                  |
                                  v
                           [USB Cable]
                                  |
                                  v
                         [RPi5 USB Port]
```

#### 3.4 Camera Module (Optional)

```
1. Locate the CSI connector (J4) on the Pi
2. Carefully open the connector latch
3. Insert the FPC cable (gold contacts facing away from the Ethernet port)
4. Close the latch securely
5. Route cable through enclosure slot
6. Mount camera module in desired position
```

### Step 4: Power Supply Setup

```
1. Connect USB-C power cable to Pi
2. Connect power supply to wall outlet
3. Do NOT power on yet!
4. Verify all connections:
   - UART connected to host PC
   - HDMI source connected
   - Audio interface connected
   - Network cable connected (optional)
5. Power on the system
```

## 📊 Final Assembly Checklist

| Component | Status | Notes |
|-----------|--------|-------|
| **Raspberry Pi 5** | [ ] | Securely mounted |
| **Active Cooler** | [ ] | Thermal paste applied, fan connected |
| **NVMe SSD** | [ ] | Detected in boot |
| **UART Console** | [ ] | Connected, working at 115200 |
| **HDMI Capture** | [ ] | Detected in `lsusb` |
| **Audio Interface** | [ ] | Detected in `arecord -l` |
| **Camera Module** | [ ] | Detected in `vcgencmd get_camera` |
| **Network** | [ ] | IP address assigned |
| **Power Supply** | [ ] | Adequate power (5V/5A) |
| **Cooling** | [ ] | Fan operational |
| **Enclosure** | [ ] ] Securely closed |

## 🖼️ Connection Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     Power Supply                            │
│                   (5V @ 5A USB-C)                          │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              Raspberry Pi 5 (Cortex-A76)                   │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │   GPIO 40-pin │  │   USB 3.0   │  │   HDMI Out   │    │
│  │    Header     │  │   Ports     │  │   (Display)  │    │
│  └──────┬───────┘  └──────┬───────┘  └──────────────┘    │
│         │                 │                                 │
│  ┌──────▼───────┐  ┌──────▼───────┐                       │
│  │   UART       │  │   HDMI       │                       │
│  │   Console    │  │   Capture    │                       │
│  │   (3-pin)    │  │   Card       │                       │
│  └──────┬───────┘  └──────┬───────┘                       │
│         │                 │                                 │
│  ┌──────▼───────┐  ┌──────▼───────┐                       │
│  │   USB to     │  │   HDMI       │                       │
│  │   Serial     │  │   Source     │                       │
│  │   Adapter    │  │   (Camera)   │                       │
│  └──────┬───────┘  └──────────────┘                       │
│         │                                                 │
│  ┌──────▼───────┐  ┌────────────────────┐               │
│  │   I2S        │  │   PCIe to SATA     │               │
│  │   Audio HAT  │  │   HAT/NVMe Base    │               │
│  └──────┬───────┘  └────────┬───────────┘               │
│         │                   │                             │
│  ┌──────▼───────┐  ┌────────▼───────────┐               │
│  │   Microphone │  │   SSD/HDD Array    │               │
│  │   Input      │  │   (NVMe)           │               │
│  └──────────────┘  └────────────────────┘               │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
                      [Network]
```

## 🎯 First Boot Procedure

### 1. Insert Boot Media
```bash
# Insert SD card with Raspberry Pi OS
# Connect all peripherals
# Power on the system
```

### 2. Check UART Console
```bash
# On host PC
screen /dev/ttyUSB0 115200

# You should see boot messages
```

### 3. Verify Components
```bash
# Check NVMe SSD
lsblk
# Should show nvme0n1

# Check HDMI Capture
lsusb
# Should show Elgato/Magewell device

# Check Audio
arecord -l
# Should show USB audio device

# Check Camera
vcgencmd get_camera
# Should show detected=1
```

### 4. Configure System
```bash
# Set hostname
sudo hostnamectl set-hostname recording-station

# Enable services
sudo systemctl enable recording-station
sudo systemctl start recording-station
```

## 📝 Common Issues & Fixes

### UART Not Working
```bash
# Check config
cat /boot/firmware/config.txt | grep uart
# Should show: enable_uart=1

# Check device
ls -l /dev/ttyAMA*
# Should show /dev/ttyAMA0 or /dev/ttyAMA10
```

### NVMe Not Detected
```bash
# Check PCIe
lspci -v
# Should show NVMe controller

# Check dmesg
dmesg | grep nvme
```

### HDMI Capture Not Detected
```bash
# Check USB
lsusb -t
# Should show in USB 3.0 bus

# Check V4L2
ls /dev/video*
# Should show /dev/video0
```

---

