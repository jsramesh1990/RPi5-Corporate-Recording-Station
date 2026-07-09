#!/bin/bash
# uart_setup.sh - Configure UART console

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Configuring UART console...${NC}"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Please run as root (sudo ./uart_setup.sh)${NC}"
    exit 1
fi

# Check if running on Raspberry Pi
if [ ! -f /proc/device-tree/model ] || ! grep -q "Raspberry Pi" /proc/device-tree/model; then
    echo -e "${YELLOW}⚠ Not running on Raspberry Pi, skipping UART configuration${NC}"
    exit 0
fi

# Configure config.txt
CONFIG_FILE="/boot/firmware/config.txt"
if [ -f "$CONFIG_FILE" ]; then
    echo -e "${YELLOW}Updating $CONFIG_FILE...${NC}"
    
    # Remove existing UART settings
    sed -i '/enable_uart/d' "$CONFIG_FILE"
    sed -i '/dtparam=uart/d' "$CONFIG_FILE"
    sed -i '/dtoverlay=uart/d' "$CONFIG_FILE"
    
    # Add UART settings
    echo -e "\n# UART Console Configuration" >> "$CONFIG_FILE"
    echo "enable_uart=1" >> "$CONFIG_FILE"
    echo "dtparam=uart0=on" >> "$CONFIG_FILE"
    echo "dtparam=uart0_console" >> "$CONFIG_FILE"
    
    # Optional: Disable Bluetooth to free UART
    # echo "dtoverlay=disable-bt" >> "$CONFIG_FILE"
    
    echo -e "${GREEN}✓ UART enabled in config.txt${NC}"
else
    echo -e "${RED}✗ $CONFIG_FILE not found${NC}"
fi

# Configure cmdline.txt
CMDLINE_FILE="/boot/firmware/cmdline.txt"
if [ -f "$CMDLINE_FILE" ]; then
    echo -e "${YELLOW}Updating $CMDLINE_FILE...${NC}"
    
    # Add console parameter if not present
    if ! grep -q "console=serial0" "$CMDLINE_FILE"; then
        sed -i 's/^/console=serial0,115200 /' "$CMDLINE_FILE"
        echo -e "${GREEN}✓ Console added to cmdline.txt${NC}"
    fi
else
    echo -e "${RED}✗ $CMDLINE_FILE not found${NC}"
fi

# Create udev rule for UART permissions
echo -e "${YELLOW}Creating udev rule for UART permissions...${NC}"
cat > /etc/udev/rules.d/99-uart.rules << EOF
KERNEL=="ttyAMA[0-9]*", MODE="0666"
KERNEL=="ttyS0", MODE="0666"
KERNEL=="ttyUSB[0-9]*", MODE="0666"
EOF

udevadm control --reload-rules
udevadm trigger

# Add user to dialout group
if command -v usermod &> /dev/null; then
    usermod -a -G dialout $SUDO_USER 2>/dev/null || true
    echo -e "${GREEN}✓ User added to dialout group${NC}"
fi

echo -e "\n${GREEN}✅ UART configuration complete${NC}"
echo -e "${YELLOW}UART Settings:${NC}"
echo -e "  Device: /dev/ttyAMA0"
echo -e "  Baud Rate: 115200"
echo -e "  Data: 8 bits"
echo -e "  Stop: 1 bit"
echo -e "  Parity: None"
echo -e "\n${YELLOW}To connect: screen /dev/ttyAMA0 115200${NC}"
echo -e "${YELLOW}Reboot recommended for changes to take effect${NC}"
