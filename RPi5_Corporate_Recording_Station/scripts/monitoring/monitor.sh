#!/bin/bash
# monitor.sh - Monitor Recording Station in real-time

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Recording Station Monitor                      ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Function to display system info
display_system_info() {
    echo -e "\n${GREEN}━━━ System Information ━━━${NC}"
    
    # CPU
    echo -e "${YELLOW}CPU:${NC}"
    echo "  Temperature: $(vcgencmd measure_temp 2>/dev/null | cut -d= -f2 || echo 'N/A')"
    echo "  Load: $(uptime | awk -F'load average:' '{print $2}' | cut -d, -f1)"
    
    # Memory
    echo -e "\n${YELLOW}Memory:${NC}"
    free -h | grep -v "Swap"
    
    # Storage
    echo -e "\n${YELLOW}Storage:${NC}"
    df -h / /var/lib/recording-station 2>/dev/null | grep -v "Filesystem"
}

# Function to display recording status
display_recording_status() {
    echo -e "\n${GREEN}━━━ Recording Status ━━━${NC}"
    
    if systemctl is-active --quiet recording-station; then
        echo -e "  Service: ${GREEN}Running${NC}"
        
        # Check if recording is active via status file
        if [ -f "/var/lib/recording-station/recording_status" ]; then
            source /var/lib/recording-station/recording_status
            if [ "$IS_RECORDING" = "true" ]; then
                echo -e "  Recording: ${RED}Active${NC}"
                echo "  File: $CURRENT_FILE"
                echo "  Duration: $CURRENT_DURATION seconds"
                echo "  Size: $CURRENT_SIZE bytes"
            else
                echo -e "  Recording: ${YELLOW}Idle${NC}"
            fi
        else
            echo -e "  Recording: ${YELLOW}Unknown${NC}"
        fi
    else
        echo -e "  Service: ${RED}Stopped${NC}"
    fi
}

# Function to display cloud status
display_cloud_status() {
    echo -e "\n${GREEN}━━━ Cloud Status ━━━${NC}"
    
    if [ -f "/var/lib/recording-station/cloud_status" ]; then
        source /var/lib/recording-station/cloud_status
        if [ "$CLOUD_CONNECTED" = "true" ]; then
            echo -e "  Status: ${GREEN}Connected${NC}"
            echo "  Provider: $CLOUD_PROVIDER"
            echo "  Syncing: $CLOUD_SYNCING"
        else
            echo -e "  Status: ${RED}Disconnected${NC}"
        fi
    else
        echo -e "  Status: ${YELLOW}Unknown${NC}"
    fi
}

# Function to display GPIO status
display_gpio_status() {
    echo -e "\n${GREEN}━━━ GPIO Status ━━━${NC}"
    if command -v gpioset &> /dev/null; then
        echo "  System LED:    $(gpioget 0 17 2>/dev/null || echo 'N/A')"
        echo "  Recording LED: $(gpioget 0 27 2>/dev/null || echo 'N/A')"
        echo "  Start Button:  $(gpioget 0 22 2>/dev/null || echo 'N/A')"
        echo "  Stop Button:   $(gpioget 0 23 2>/dev/null || echo 'N/A')"
        echo "  Error LED:     $(gpioget 0 24 2>/dev/null || echo 'N/A')"
        echo "  Status LED:    $(gpioget 0 25 2>/dev/null || echo 'N/A')"
    else
        echo "  GPIO tools not available"
    fi
}

# Main loop
INTERVAL=${1:-5}
echo -e "${YELLOW}Monitoring every $INTERVAL seconds (Ctrl+C to exit)${NC}"

while true; do
    clear
    display_system_info
    display_recording_status
    display_cloud_status
    display_gpio_status
    
    echo -e "\n${BLUE}━━━ Press Ctrl+C to exit ━━━${NC}"
    sleep "$INTERVAL"
done
