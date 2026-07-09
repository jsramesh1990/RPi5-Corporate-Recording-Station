# SPI Device Mapping
## Raspberry Pi 5 Recording Station

## 📋 Overview

The Recording Station uses SPI for various high-speed peripherals. This document lists all SPI devices and their configurations.

## 🔌 SPI Buses

### SPI0 (Default)
| Pin | Function | GPIO |
|-----|----------|------|
| 19 | MOSI | 10 |
| 21 | MISO | 9 |
| 23 | SCLK | 11 |
| 24 | CE0 | 8 |
| 26 | CE1 | 7 |

### SPI1 (Auxiliary)
| Pin | Function | GPIO |
|-----|----------|------|
| 38 | MOSI | 20 |
| 35 | MISO | 19 |
| 12 | SCLK | 18 |
| 36 | CE0 | 16 |
| 11 | CE1 | 17 |

### SPI2 (Auxiliary)
| Pin | Function | GPIO |
|-----|----------|------|
| 3 | MOSI | 2 |
| 5 | MISO | 3 |
| 4 | SCLK | 4 |
| 2 | CE0 | 5 |
| 0 | CE1 | 6 |

## 📊 Device Configuration

### SPI0 Devices
| CE | Device | Description | Speed |
|----|--------|-------------|-------|
| 0 | Display | SPI Display | 20 MHz |
| 0 | ADC | SPI ADC | 10 MHz |
| 0 | DAC | SPI DAC | 10 MHz |
| 1 | Sensor | SPI Sensor | 5 MHz |
| 1 | SD Card | SPI SD | 20 MHz |

### SPI1 Devices
| CE | Device | Description | Speed |
|----|--------|-------------|-------|
| 0 | Audio | SPI Audio | 10 MHz |
| 1 | Encoder | SPI Encoder | 10 MHz |

## 🛠️ SPI Commands

### Enable SPI
```bash
# Edit config.txt
sudo nano /boot/firmware/config.txt

# Add:
dtparam=spi=on
dtoverlay=spi1-1cs
dtoverlay=spi2-1cs
```

### List Devices
```bash
# Check SPI devices
ls -l /dev/spi*

# Should show:
# /dev/spidev0.0
# /dev/spidev0.1
```

### Test SPI
```bash
# Install tools
sudo apt install -y spidev-test

# Test SPI0 CE0
./spidev_test -D /dev/spidev0.0

# Test SPI0 CE1
./spidev_test -D /dev/spidev0.1
```

## 📊 SPI Specifications

| Parameter | Value |
|-----------|-------|
| Max Speed | 125 MHz |
| Modes | 0, 1, 2, 3 |
| Bits/Word | 8, 16, 32 |
| Voltage | 3.3V |
| Clock Polarity | CPOL 0/1 |
| Clock Phase | CPHA 0/1 |

## 🐛 Troubleshooting

### SPI Not Working
```bash
# Check configuration
cat /boot/firmware/config.txt | grep spi

# Check devices
ls -l /dev/spi*

# Check dmesg
dmesg | grep spi

# Check wiring
# Verify MOSI, MISO, SCLK connections
```

### Communication Errors
```bash
# Test with loopback
# Connect MOSI to MISO

# Run test
./spidev_test -D /dev/spidev0.0

# Should see:
# RX | 00 00 00 00 00 00 ...
```

---

