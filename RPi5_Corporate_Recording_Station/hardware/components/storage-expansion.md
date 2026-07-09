# Storage Expansion Guide
## RPi5 Corporate Recording Station

## 📋 Overview

This guide covers storage expansion options for the Raspberry Pi 5 Corporate Recording Station. Proper storage is critical for reliable recording and archiving.

## 🎯 Recommended Solutions

### 1. NVMe SSD (Best Performance)

#### Pimoroni NVMe Base
- **Price**: $25
- **Interface**: PCIe 2.0 x1
- **Form Factor**: M.2 2230/2242
- **Speed**: Up to 800 MB/s
- **Features**:
  - Bootable
  - Low profile
  - No power adapter needed

**Compatible SSDs**:
```
Kingston NV2 250GB: $30
Western Digital SN740 256GB: $40
Samsung PM991 256GB: $45
Crucial P3 500GB: $50
Sabrent Rocket 1TB: $100
```

#### NVMe to USB Adapter
- **Price**: $15-30
- **Interface**: USB 3.0/3.1
- **Form Factor**: M.2 2230/2242/2280
- **Speed**: Up to 450 MB/s
- **Features**:
  - Portable
  - No internal installation

### 2. SATA Solutions

#### SATA HAT (X829)
- **Price**: $50
- **Interface**: PCIe 2.0 x1
- **Support**: 2x SATA III
- **Features**:
  - RAID 0/1/JBOD
  - 5V power
  - Stackable

#### USB SATA Adapter
- **Price**: $10-20
- **Interface**: USB 3.0
- **Support**: 1x SATA III
- **Features**:
  - Simple
  - Portable

### 3. Enterprise Solutions

#### NAS Integration
- **Price**: $200+
- **Interface**: Network
- **Support**: Multiple drives
- **Features**:
  - RAID protection
  - Redundancy
  - Remote access

## 📊 Storage Specifications

### Performance Comparison

| Storage Type | Read Speed | Write Speed | Capacity | Cost/GB |
|--------------|------------|-------------|----------|---------|
| NVMe SSD | 800 MB/s | 600 MB/s | 256GB-2TB | $0.10 |
| SATA SSD | 500 MB/s | 450 MB/s | 256GB-4TB | $0.08 |
| USB SSD | 400 MB/s | 350 MB/s | 128GB-2TB | $0.07 |
| HDD | 100 MB/s | 80 MB/s | 1TB-12TB | $0.03 |
| MicroSD | 80 MB/s | 60 MB/s | 32GB-1TB | $0.15 |

### Recording Capacity Estimates

| Format | Bitrate | 1 Hour | 4 Hours | 8 Hours | 1 Day |
|--------|---------|--------|---------|---------|-------|
| 1080p 30fps | 4 Mbps | 1.8 GB | 7.2 GB | 14.4 GB | 43.2 GB |
| 1080p 60fps | 8 Mbps | 3.6 GB | 14.4 GB | 28.8 GB | 86.4 GB |
| 4K 30fps | 20 Mbps | 9 GB | 36 GB | 72 GB | 216 GB |
| 4K 60fps | 40 Mbps | 18 GB | 72 GB | 144 GB | 432 GB |

## 🔧 Installation Guide

### NVMe Base Installation

```
1. Power off Pi
2. Locate PCIe connector (J7)
3. Connect FPC ribbon cable:
   - Gold contacts facing down
   - Secure with latch
4. Mount NVMe Base on GPIO
5. Install SSD:
   - Insert at 30-degree angle
   - Press down
   - Secure with screw
6. Power on
7. Check detection: lsblk
```

### SATA HAT Installation

```
1. Power off Pi
2. Install HAT on GPIO
3. Connect SATA cables:
   - SATA 1: Primary drive
   - SATA 2: Secondary drive
4. Connect power:
   - 5V via GPIO
   - External supply if needed
5. Install drives
6. Power on
7. Configure RAID if needed
```

### USB Storage Setup

```
1. Connect drive to USB port
2. Check detection: lsblk
3. Format if needed
4. Mount to /mnt/storage
5. Add to fstab
```

## 🔧 Configuration

### NVMe Optimization
```bash
# Enable PCIe
echo "dtoverlay=pcie-32bit-dma" >> /boot/firmware/config.txt

# Set IO scheduler
echo mq-deadline > /sys/block/nvme0n1/queue/scheduler

# Enable TRIM
sudo systemctl enable fstrim.timer
sudo systemctl start fstrim.timer
```

### RAID Configuration
```bash
# Install mdadm
sudo apt install -y mdadm

# Create RAID 1 (mirror)
sudo mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/sda1 /dev/sdb1

# Create filesystem
sudo mkfs.ext4 /dev/md0

# Mount
sudo mkdir -p /mnt/raid
sudo mount /dev/md0 /mnt/raid
```

### Auto-mount Setup
```bash
# Get UUID
sudo blkid

# Add to fstab
echo "UUID=your-uuid /mnt/storage ext4 defaults,noatime 0 2" >> /etc/fstab

# Test mount
sudo mount -a
```

## 📊 Performance Tuning

### Benchmark Tests
```bash
# Write test
sudo dd if=/dev/zero of=/mnt/storage/test bs=1M count=1024 conv=fdatasync

# Read test
sudo dd if=/mnt/storage/test of=/dev/null bs=1M count=1024

# Random I/O
sudo fio --filename=/mnt/storage/test --size=1G --direct=1 --rw=randwrite --bs=4k --numjobs=4 --runtime=60
```

### File System Optimization
```bash
# For NVMe
sudo mount -o discard,noatime,compress=no /dev/nvme0n1p1 /mnt/storage

# For HDD
sudo mount -o noatime,barrier=0 /dev/sda1 /mnt/storage

# For USB
sudo mount -o noatime,nodiratime /dev/sda1 /mnt/storage
```

## 🐛 Troubleshooting

### NVMe Not Detected
```bash
# Check PCIe
lspci -v

# Check dmesg
dmesg | grep nvme

# Check config
cat /boot/firmware/config.txt | grep pcie

# Try different SSD
# Check FPC cable
```

### Poor Performance
```bash
# Check speed
sudo hdparm -tT /dev/nvme0n1

# Check SMART
sudo smartctl -a /dev/nvme0n1

# Check temperature
sudo hdparm -C /dev/nvme0n1

# Check for TRIM support
sudo lsblk -D
```

### Data Integrity
```bash
# Check filesystem
sudo fsck /dev/nvme0n1p1

# Check for errors
sudo dmesg | grep -i error

# Check S.M.A.R.T.
sudo smartctl -a /dev/nvme0n1
```

---

## 📊 Storage Calculator

### Capacity Planning Tool
```bash
#!/bin/bash
# storage-calculator.sh

echo "Storage Capacity Calculator"
echo "=========================="
echo ""

# Input recording hours per day
read -p "Hours of recording per day: " hours_per_day

# Input bitrate
read -p "Bitrate (Mbps): " bitrate

# Calculate daily storage
daily_gb=$((hours_per_day * bitrate * 3600 / 8000))
echo "Daily storage: $daily_gb GB"

# Calculate monthly
monthly_gb=$((daily_gb * 30))
echo "Monthly storage: $monthly_gb GB"

# Calculate recommended drive size
recommended=$((monthly_gb * 2))
echo "Recommended drive size: $recommended GB"
```

---

