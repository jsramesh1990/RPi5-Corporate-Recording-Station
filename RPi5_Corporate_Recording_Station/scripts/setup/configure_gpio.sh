#!/bin/bash
# configure_gpio.sh - Configure GPIO pins for Recording Station

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Configuring GPIO pins...${NC}"

# Check if running on Raspberry Pi
if [ ! -f /proc/device-tree/model ] || ! grep -q "Raspberry Pi" /proc/device-tree/model; then
    echo -e "${YELLOW}⚠ Not running on Raspberry Pi, skipping GPIO configuration${NC}"
    exit 0
fi

# Install gpio tools if not available
if ! command -v gpioset &> /dev/null; then
    echo -e "${YELLOW}Installing gpio tools...${NC}"
    apt-get install -y -qq gpiod
fi

# GPIO pin definitions
GPIO_SYSTEM_LED=17
GPIO_RECORDING_LED=27
GPIO_START_BUTTON=22
GPIO_STOP_BUTTON=23
GPIO_ERROR_LED=24
GPIO_STATUS_LED=25

# Export GPIO pins
echo -e "${YELLOW}Exporting GPIO pins...${NC}"
for pin in $GPIO_SYSTEM_LED $GPIO_RECORDING_LED $GPIO_START_BUTTON $GPIO_STOP_BUTTON $GPIO_ERROR_LED $GPIO_STATUS_LED; do
    if [ -d "/sys/class/gpio/gpio$pin" ]; then
        echo "$pin" > /sys/class/gpio/unexport 2>/dev/null || true
    fi
    echo "$pin" > /sys/class/gpio/export 2>/dev/null || true
done

# Configure direction
echo -e "${YELLOW}Configuring pin directions...${NC}"
for pin in $GPIO_SYSTEM_LED $GPIO_RECORDING_LED $GPIO_ERROR_LED $GPIO_STATUS_LED; do
    echo "out" > /sys/class/gpio/gpio$pin/direction 2>/dev/null || true
done

for pin in $GPIO_START_BUTTON $GPIO_STOP_BUTTON; do
    echo "in" > /sys/class/gpio/gpio$pin/direction 2>/dev/null || true
done

# Set initial state
echo -e "${YELLOW}Setting initial pin states...${NC}"
gpioset 0 $GPIO_SYSTEM_LED=0 2>/dev/null || true
gpioset 0 $GPIO_RECORDING_LED=0 2>/dev/null || true
gpioset 0 $GPIO_ERROR_LED=0 2>/dev/null || true
gpioset 0 $GPIO_STATUS_LED=0 2>/dev/null || true

# Enable GPIO interrupts for buttons
echo -e "${YELLOW}Enabling GPIO interrupts...${NC}"
for pin in $GPIO_START_BUTTON $GPIO_STOP_BUTTON; do
    echo "both" > /sys/class/gpio/gpio$pin/edge 2>/dev/null || true
done

# Create udev rule for GPIO permissions
echo -e "${YELLOW}Creating udev rule for GPIO permissions...${NC}"
cat > /etc/udev/rules.d/99-gpio.rules << EOF
SUBSYSTEM=="gpio", ACTION=="add", RUN+="/bin/chmod 666 /sys/class/gpio/export /sys/class/gpio/unexport"
SUBSYSTEM=="gpio", ACTION=="add", RUN+="/bin/chmod 666 /sys/class/gpio/gpio*/value"
SUBSYSTEM=="gpio", ACTION=="add", RUN+="/bin/chmod 666 /sys/class/gpio/gpio*/direction"
EOF

# Reload udev rules
udevadm control --reload-rules
udevadm trigger

# Create GPIO status script
cat > /usr/local/bin/gpio_status.sh << 'EOF'
#!/bin/bash
echo "GPIO Status:"
echo "============"
echo "System LED (17):   $(gpioset 0 17=0 2>/dev/null; gpioget 0 17 2>/dev/null || echo 'N/A')"
echo "Recording LED (27): $(gpioset 0 27=0 2>/dev/null; gpioget 0 27 2>/dev/null || echo 'N/A')"
echo "Start Button (22):  $(gpioget 0 22 2>/dev/null || echo 'N/A')"
echo "Stop Button (23):   $(gpioget 0 23 2>/dev/null || echo 'N/A')"
echo "Error LED (24):    $(gpioget 0 24 2>/dev/null || echo 'N/A')"
echo "Status LED (25):   $(gpioget 0 25 2>/dev/null || echo 'N/A')"
EOF
chmod +x /usr/local/bin/gpio_status.sh

echo -e "${GREEN}✅ GPIO configuration complete${NC}"
echo -e "${GREEN}📊 Run 'gpio_status.sh' to check pin states${NC}"
