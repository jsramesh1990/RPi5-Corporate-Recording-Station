# GPIO Pinout Reference
## Raspberry Pi 5 (BCM2712)

## 📋 Overview

The Raspberry Pi 5 uses a 40-pin GPIO header (J8) with the BCM2712 processor. All pins are 3.3V logic level and 5V tolerant (except GPIO 0-1).

## 📊 Complete Pin Mapping

| Pin# | Name | Function | BCM | Description |
|------|------|----------|-----|-------------|
| 1 | 3V3 | Power | - | 3.3V Power (max 300mA) |
| 2 | 5V | Power | - | 5V Power (max 1.2A) |
| 3 | GPIO2 | I2C1 SDA | 2 | I2C Data (1.8V pull-up) |
| 4 | 5V | Power | - | 5V Power |
| 5 | GPIO3 | I2C1 SCL | 3 | I2C Clock (1.8V pull-up) |
| 6 | GND | Ground | - | Ground |
| 7 | GPIO4 | GPIO | 4 | General Purpose I/O |
| 8 | GPIO14 | UART0 TX | 14 | Primary UART Transmit |
| 9 | GND | Ground | - | Ground |
| 10 | GPIO15 | UART0 RX | 15 | Primary UART Receive |
| 11 | GPIO17 | GPIO | 17 | General Purpose I/O |
| 12 | GPIO18 | PCM CLK | 18 | Audio Clock / PWM0 |
| 13 | GPIO27 | GPIO | 27 | General Purpose I/O |
| 14 | GND | Ground | - | Ground |
| 15 | GPIO22 | GPIO | 22 | General Purpose I/O |
| 16 | GPIO23 | GPIO | 23 | General Purpose I/O |
| 17 | 3V3 | Power | - | 3.3V Power |
| 18 | GPIO24 | GPIO | 24 | General Purpose I/O |
| 19 | GPIO10 | SPI0 MOSI | 10 | SPI Master Out Slave In |
| 20 | GND | Ground | - | Ground |
| 21 | GPIO9 | SPI0 MISO | 9 | SPI Master In Slave Out |
| 22 | GPIO25 | GPIO | 25 | General Purpose I/O |
| 23 | GPIO11 | SPI0 SCLK | 11 | SPI Clock |
| 24 | GPIO8 | SPI0 CE0 | 8 | SPI Chip Enable 0 |
| 25 | GND | Ground | - | Ground |
| 26 | GPIO7 | SPI0 CE1 | 7 | SPI Chip Enable 1 |
| 27 | GPIO0 | ID_SD | 0 | I2C ID EEPROM |
| 28 | GPIO1 | ID_SC | 1 | I2C ID EEPROM Clock |
| 29 | GPIO5 | GPIO | 5 | General Purpose I/O |
| 30 | GND | Ground | - | Ground |
| 31 | GPIO6 | GPIO | 6 | General Purpose I/O |
| 32 | GPIO12 | PWM0 | 12 | Pulse Width Modulation |
| 33 | GPIO13 | PWM1 | 13 | Pulse Width Modulation |
| 34 | GND | Ground | - | Ground |
| 35 | GPIO19 | PCM_FS | 19 | Audio Frame Sync |
| 36 | GPIO16 | GPIO | 16 | General Purpose I/O |
| 37 | GPIO26 | GPIO | 26 | General Purpose I/O |
| 38 | GPIO20 | PCM_DIN | 20 | Audio Data In |
| 39 | GND | Ground | - | Ground |
| 40 | GPIO21 | PCM_DOUT | 21 | Audio Data Out |

## 🎯 Recording Station Pin Allocation

| Pin | GPIO | Function | Direction | Status LED |
|-----|------|----------|-----------|------------|
| 8 | 14 | UART0 TX | Output | Console output |
| 10 | 15 | UART0 RX | Input | Console input |
| 11 | 17 | System LED | Output | System ready |
| 13 | 27 | Recording LED | Output | Recording active |
| 15 | 22 | Start Button | Input | Start recording |
| 16 | 23 | Stop Button | Input | Stop recording |
| 18 | 24 | Status LED | Output | Error status |

## 🔌 GPIO Power Specifications

| Supply | Voltage | Max Current | Notes |
|--------|---------|-------------|-------|
| 3V3 | 3.3V | 300mA | Total across all pins |
| 5V | 5.0V | 1.2A | Direct from USB-C |
| GND | 0V | - | Reference ground |

## 📊 GPIO Electrical Characteristics

| Parameter | Min | Typ | Max | Unit |
|-----------|-----|-----|-----|------|
| Input Low Voltage | - | - | 0.8 | V |
| Input High Voltage | 2.0 | - | - | V |
| Output Low Voltage | - | - | 0.4 | V |
| Output High Voltage | 2.8 | - | - | V |
| Output Current | - | 8 | 16 | mA |
| Input Leakage | - | - | 10 | uA |
| Pull-up Resistance | 50 | 55 | 60 | kΩ |
| Pull-down Resistance | 50 | 55 | 60 | kΩ |

## 🛠️ GPIO Usage Examples

### Reading Button State
```bash
# Check button on GPIO 22
gpioset 0 22=0  # Set as input
gpioget 0 22    # Read value
```

### Controlling LED
```bash
# Turn on system LED (GPIO 17)
gpioset 0 17=1

# Turn off recording LED (GPIO 27)
gpioset 0 27=0

# Toggle LED
gpioset 0 17=toggle
```

### GPIO Interrupts
```python
import RPi.GPIO as GPIO

GPIO.setmode(GPIO.BCM)
GPIO.setup(22, GPIO.IN, pull_up_down=GPIO.PUD_UP)

def button_callback(channel):
    print("Button pressed!")

GPIO.add_event_detect(22, GPIO.FALLING, callback=button_callback, bouncetime=300)
```

---

