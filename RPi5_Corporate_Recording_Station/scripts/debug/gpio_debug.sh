#!/bin/bash
# gpio_debug.sh - Debug GPIO pins

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     GPIO Debug Tool                                      ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Check if gpio tools are available
if ! command -v gpioset &> /dev/null; then
    echo -e "${RED}❌ GPIO tools not found. Install: apt-get install gpiod${NC}"
    exit 1
fi

# GPIO pins to test
GPIO_PINS="17 27 22 23 24 25"
PIN_NAMES="System_LED Recording_LED Start_Button Stop_Button Error_LED Status_LED"

echo -e "${YELLOW}Testing GPIO pins...${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Test each pin
for pin in $GPIO_PINS; do
    # Get pin name
    name=$(echo $PIN_NAMES | cut -d' ' -f$(echo $GPIO_PINS | tr ' ' '\n' | grep -n "^$pin$" | cut -d: -f1))
    
    echo -e "\n${YELLOW}Testing GPIO $pin ($name)...${NC}"
    
    # Check if pin exists
    if [ -d "/sys/class/gpio/gpio$pin" ]; then
        echo -e "  ${GREEN}✓${NC} Pin exported"
        
        # Read current value
        value=$(cat /sys/class/gpio/gpio$pin/value 2>/dev/null || echo "N/A")
        echo -e "  Current value: $value"
        
        # If output pin, test toggling
        direction=$(cat /sys/class/gpio/gpio$pin/direction 2>/dev/null || echo "N/A")
        echo -e "  Direction: $direction"
        
        if [ "$direction" = "out" ]; then
            echo -e "  ${YELLOW}Testing output...${NC}"
            echo 0 > /sys/class/gpio/gpio$pin/value
            sleep 0.5
            echo 1 > /sys/class/gpio/gpio$pin/value
            sleep 0.5
            echo 0 > /sys/class/gpio/gpio$pin/value
            echo -e "  ${GREEN}✓${NC} Output test passed"
        elif [ "$direction" = "in" ]; then
            echo -e "  ${YELLOW}Input pin (press button to test)${NC}"
            echo -e "  Press button connected to GPIO $pin..."
            
            # Monitor for 2 seconds
            for i in {1..20}; do
                value=$(cat /sys/class/gpio/gpio$pin/value)
                echo -ne "\r  Value: $value   "
                sleep 0.1
            done
            echo ""
        fi
    else
        echo -e "  ${RED}✗${NC} Pin not exported"
    fi
done

echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✅ GPIO debug complete${NC}"
