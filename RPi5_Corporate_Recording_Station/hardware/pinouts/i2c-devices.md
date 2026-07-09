# I2C Device Addresses
## Raspberry Pi 5 Recording Station

## 📋 Overview

The Recording Station uses I2C for various sensors and peripherals. This document lists all I2C addresses and their functions.

## 🔌 I2C Buses

| Bus | Device | Pins | Purpose |
|-----|--------|------|---------|
| I2C0 | /dev/i2c-0 | GPIO 0/1 | ID EEPROM |
| I2C1 | /dev/i2c-1 | GPIO 2/3 | Camera control |
| I2C2 | /dev/i2c-2 | GPIO 4/5 | Sensors |
| I2C3 | /dev/i2c-3 | GPIO 6/7 | Expansion |
| I2C4 | /dev/i2c-4 | GPIO 8/9 | Expansion |
| I2C5 | /dev/i2c-5 | GPIO 12/13 | Expansion |
| I2C6 | /dev/i2c-6 | GPIO 22/23 | Expansion |

## 📊 Device Address Map

### System Devices
| Address | Device | Function | Bus |
|---------|--------|----------|-----|
| 0x50 | EEPROM | ID EEPROM | 0 |
| 0x51 | EEPROM | ID EEPROM | 0 |

### Camera Devices
| Address | Device | Function | Bus |
|---------|--------|----------|-----|
| 0x1a | Camera | IMX708/IMX219 | 1 |
| 0x1c | Camera | Camera control | 1 |
| 0x36 | Camera | Power management | 1 |

### Sensor Devices
| Address | Device | Type | Bus |
|---------|--------|------|-----|
| 0x18 | Temperature | LM75 | 2 |
| 0x19 | Temperature | TMP102 | 2 |
| 0x1e | Accelerometer | LSM303 | 2 |
| 0x28 | Barometer | BMP280 | 2 |
| 0x29 | Magnetometer | HMC5883L | 2 |
| 0x40 | Humidity | SHT31 | 2 |
| 0x48 | ADC | ADS1115 | 2 |
| 0x50 | DAC | MCP4725 | 2 |
| 0x5a | Light | TSL2561 | 2 |
| 0x68 | RTC | DS3231 | 2 |
| 0x77 | Barometer | BME280 | 2 |

### Expansion Devices
| Address | Device | Function | Bus |
|---------|--------|----------|-----|
| 0x20 | GPIO Expander | MCP23017 | 3 |
| 0x21 | GPIO Expander | MCP23017 | 3 |
| 0x22 | PWM Driver | PCA9685 | 3 |
| 0x23 | PWM Driver | PCA9685 | 3 |
| 0x60 | OLED Display | SSD1306 | 3 |
| 0x61 | OLED Display | SSD1306 | 3 |
| 0x70 | LED Driver | HT16K33 | 3 |

## 🛠️ I2C Commands

### Scan for Devices
```bash
# Scan all buses
for bus in 0 1 2 3 4 5 6; do
    echo "Bus $bus:"
    i2cdetect -y $bus
done

# Scan specific bus
i2cdetect -y 1
```

### Read from Device
```bash
# Read 8 bytes from address 0x1a on bus 1
i2cget -y 1 0x1a 0x00

# Read multiple bytes
i2cget -y 1 0x1a 0x00 w
```

### Write to Device
```bash
# Write byte to address 0x1a on bus 1
i2cset -y 1 0x1a 0x00 0x01

# Write multiple bytes
i2cset -y 1 0x1a 0x00 0x01 0x02 i
```

## 📊 I2C Specifications

| Parameter | Value |
|-----------|-------|
| Max Speed | 400 kHz (Fast Mode) |
| Voltage | 3.3V (1.8V for camera) |
| Pull-up Resistors | 1.8kΩ (camera), 4.7kΩ (others) |
| Bus Capacitance | Max 400pF |
| Max Devices | 127 per bus |

## 🔧 Configuration

### Enable I2C
```bash
# Edit config.txt
sudo nano /boot/firmware/config.txt

# Add:
dtparam=i2c_arm=on

# Disable I2C (if needed)
# dtparam=i2c_arm=off
```

### Install Tools
```bash
# Install I2C tools
sudo apt install -y i2c-tools python3-smbus

# Add user to i2c group
sudo usermod -a -G i2c $USER
```

## 🐛 Troubleshooting

### Device Not Found
```bash
# Check physical connection
# Check pull-up resistors

# Scan bus
i2cdetect -y 1

# Check dmesg
dmesg | grep i2c

# Check voltage
# I2C devices need 3.3V or 1.8V
```

### Communication Errors
```bash
# Check address
i2cdetect -y 1 | grep -i "1a"

# Check bus speed
i2cget -y 1 0x1a 0x00

# Check wiring
# SDA = GPIO 2, SCL = GPIO 3
```

---

