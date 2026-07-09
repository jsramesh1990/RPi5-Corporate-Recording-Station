## 🔧 Complete Build & Deployment Procedure for RPi5 Recording Station

Here's the complete procedure to build, flash, and run your corporate recording station code on a real Raspberry Pi 5 board.

---

## 📋 Prerequisites

### Hardware Required
| Component | Specification | Purpose |
|-----------|--------------|---------|
| **Raspberry Pi 5** | 4GB/8GB RAM | Main board |
| **MicroSD Card** | 32GB+ Class 10/U3 | Operating system |
| **USB-C Power Supply** | 5V/5A (27W) | Power  |
| **USB-to-UART Adapter** | CP2102/FTDI | Serial console  |
| **Host PC** | Linux/Windows/Mac | Development machine |
| **Network Cable** | Ethernet | Stable connection  |

---

## 🏗️ Build Procedure

### Step 1: Set Up Cross-Compilation Environment

The Raspberry Pi 5 uses an **ARM Cortex-A76 (aarch64)** architecture, so you must cross-compile on your host machine .

**On Ubuntu/Debian host:**
```bash
# Install cross-compilation toolchain
sudo apt update
sudo apt install -y g++-aarch64-linux-gnu binutils-aarch64-linux-gnu
sudo apt install -y build-essential cmake git

# Verify installation
aarch64-linux-gnu-g++ --version
```

**For the complete project with CMake:**
```bash
# Clone the repository
git clone https://github.com/yourusername/rpi5-corporate-recording-station.git
cd rpi5-corporate-recording-station

# Build with CMake (using ARM64 toolchain)
mkdir -p build && cd build
cmake .. -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
         -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
         -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Step 2: Install Raspberry Pi OS

Use **Raspberry Pi Imager** or manual method :

**Option A - Using Raspberry Pi Imager (Recommended):**
```bash
# Install Raspberry Pi Imager
sudo apt install rpi-imager

# Launch and select:
# - Raspberry Pi OS (64-bit) or Raspberry Pi OS Lite (64-bit)
# - Choose your SD card
# - Click "Write"
```

**Option B - Manual flashing:**
```bash
# Download Raspberry Pi OS 64-bit
wget https://downloads.raspberrypi.org/raspios_lite_arm64_latest

# Flash to SD card (replace /dev/sdX with your device)
sudo dd if=raspios_lite_arm64_latest.img of=/dev/sdX bs=4M status=progress

# Or use:
sudo dd if=raspios_lite_arm64_latest.img of=/dev/sdX bs=4M conv=fsync
```

---

## 🔌 Enabling UART Console

### Critical: Raspberry Pi 5 UART Configuration

The Raspberry Pi 5 has a **dedicated 3-pin UART connector** next to the HDMI ports. Recent EEPROM updates affect which UART device is used .

**Edit `/boot/firmware/config.txt`:**

```bash
# Mount the SD card's boot partition
# On host, if card is mounted at /mnt/boot:
sudo nano /mnt/boot/firmware/config.txt
```

**Add/modify these lines:**
```
enable_uart=1                    # Enables UART console 
console=serial0,115200          # In cmdline.txt for kernel console 

# For GPIO 14 & 15 (pins 8 & 10 on 40-pin header):
dtparam=uart0                   # Enable UART0/ttyAMA0 on GPIO 14 & 15 
dtparam=uart0_console           # Make it the console UART 

# For dedicated 3-pin connector (default on Pi5):
# Comment out the above two lines to keep console on /dev/ttyAMA10
```

**Important**: The dedicated connector uses `/dev/ttyAMA10`, while GPIO pins use `/dev/ttyAMA0`. Recent EEPROM changes mean `enable_uart=1` now moves the console to GPIO pins .

**Edit `/boot/firmware/cmdline.txt`:**
```bash
sudo nano /mnt/boot/firmware/cmdline.txt
```

**Ensure it contains:**
```
console=serial0,115200 console=tty1 root=PARTUUID=... rootfstype=ext4 fsck.repair=yes rootwait quiet splash
```

Remove `quiet` to see boot messages on the serial console .

---

## 📲 Flashing the Application

### Step 1: Deploy the Compiled Binary

```bash
# Copy the compiled application to the SD card
# If SD card is mounted at /mnt/rootfs:
sudo cp build/recording-station /mnt/rootfs/usr/local/bin/

# Or use scp after booting the Pi
scp build/recording-station pi@<raspberry-pi-ip>:/home/pi/
```

### Step 2: Install Systemd Service (Autostart)

**Create service file:**
```bash
# On the Raspberry Pi
sudo nano /etc/systemd/system/recording-station.service
```

**Content:**
```ini
[Unit]
Description=RPi5 Corporate Recording Station
After=network.target sound.target multi-user.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/recording-station
ExecStart=/usr/local/bin/recording-station
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
EnvironmentFile=/etc/recording-station/recording-station.conf

[Install]
WantedBy=multi-user.target
```

**Enable and start:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable recording-station
sudo systemctl start recording-station
sudo systemctl status recording-station
```

---

## 🔌 Hardware Connections

### UART Console Wiring 

| USB-to-UART Adapter | Raspberry Pi 5 Pin | GPIO Pin |
|---------------------|-------------------|----------|
| **GND** (Black) | Pin 6 | GND |
| **RX** (White) | Pin 8 | GPIO 14 (UART0 TX) |
| **TX** (Green) | Pin 10 | GPIO 15 (UART0 RX) |

**For dedicated 3-pin connector:**
| Connector Pin | Signal |
|---------------|--------|
| 1 | TX (to adapter's RX) |
| 2 | RX (to adapter's TX) |
| 3 | GND |

### Serial Connection Settings 
```bash
# On host PC (Linux):
tio -b 115200 /dev/ttyUSB0

# Or using screen:
screen /dev/ttyUSB0 115200 8N1

# With minicom:
minicom -D /dev/ttyUSB0 -b 115200
```

---

## 🚀 Running on Real Board

### Complete Boot Sequence

1. **Insert the SD card** into the Raspberry Pi 5
2. **Connect the UART adapter** as shown above
3. **Open a terminal** on your host: `screen /dev/ttyUSB0 115200`
4. **Power on** the Pi with USB-C (5V/5A)
5. **Observe boot messages** on the serial console

**Expected UART output:**
```
Raspberry Pi 5 (Cortex-A76) booting...
[  0.000000] Booting Linux on physical CPU 0x0000000000 [0x414fd0b1]
[  0.000000] Linux version 6.1.0-rpi7-rpi-2712
...
[  2.345678] Recording Station starting...
✅ All systems initialized!
🌐 Web Dashboard: http://192.168.1.100:8080
🔌 UART Console active
📹 Recording ready
```

### Interacting via UART Console 

Once the system is running, you can send commands via UART:

```bash
# In your serial terminal:
status                    # Check system status
record                    # Start recording
stop                      # Stop recording
upload                    # Upload to cloud
memory                    # Show memory usage
storage                   # Show storage usage
help                      # List all commands
```

---

## 📊 System Monitoring

### On the Pi (via SSH or UART):
```bash
# Check system status
sudo systemctl status recording-station

# View logs
sudo journalctl -u recording-station -f

# Check memory
htop
free -h

# Check storage
df -h /var/lib/recording-station

# Check GPIO status
gpio readall
```

### Access Web Dashboard
```bash
# On your browser (from any device on network):
http://<raspberry-pi-ip>:8080
```

---

## 🐛 Troubleshooting

### UART Not Working 
```bash
# Check device file
ls -l /dev/ttyAMA*

# Verify config
cat /boot/firmware/config.txt | grep uart
cat /boot/firmware/cmdline.txt

# If using GPIO 14/15, ensure EEPROM is updated:
sudo rpi-eeprom-update
```

### Cross-Compilation Issues 
```bash
# Verify binary architecture
file build/recording-station
# Should show: ELF 64-bit LSB executable, ARM aarch64

# Check dependencies (on Pi)
ldd /usr/local/bin/recording-station
```

### SD Card Boot Issues
```bash
# Try with initial_turbo=0 in config.txt
echo "initial_turbo=0" >> /boot/firmware/config.txt 
```

---

## ✅ Verification Checklist

| Step | Status | Command |
|------|--------|---------|
| **Cross-compilation working** | [ ] | `aarch64-linux-gnu-g++ --version` |
| **Binary compiled** | [ ] | `file build/recording-station` |
| **SD card flashed** | [ ] | `lsblk` shows partitions |
| **UART configured** | [ ] | `cat /boot/firmware/config.txt` |
| **Serial connection** | [ ] | `screen /dev/ttyUSB0 115200` |
| **System boots** | [ ] | Boot messages visible on UART |
| **Service running** | [ ] | `systemctl status recording-station` |
| **Web dashboard** | [ ] | `curl http://localhost:8080` |

---

## 📦 Quick Deployment Script

```bash
#!/bin/bash
# deploy.sh - Complete deployment script

echo "🔧 Building project..."
mkdir -p build && cd build
aarch64-linux-gnu-g++ -std=c++17 -O2 -march=armv8.2-a+crypto \
    ../src/*.cpp -o recording-station -pthread

echo "📲 Deploying to Pi..."
scp recording-station pi@$PI_IP:/home/pi/

echo "🔌 Configuring UART..."
ssh pi@$PI_IP "sudo sed -i '/enable_uart/d' /boot/firmware/config.txt"
ssh pi@$PI_IP "echo 'enable_uart=1' | sudo tee -a /boot/firmware/config.txt"
ssh pi@$PI_IP "echo 'dtparam=uart0' | sudo tee -a /boot/firmware/config.txt"
ssh pi@$PI_IP "echo 'dtparam=uart0_console' | sudo tee -a /boot/firmware/config.txt"

echo "🔄 Rebooting Pi..."
ssh pi@$PI_IP "sudo reboot"

echo "✅ Deployment complete!"
echo "🔌 Connect to UART: screen /dev/ttyUSB0 115200"
```

This complete procedure ensures your `rpi5-corporate-recording-station` project runs flawlessly on real hardware, with proper UART console access for debugging and monitoring!
