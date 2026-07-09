#!/bin/bash
# health_check.sh - System health check for Recording Station

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

HEALTH_FILE="/var/lib/recording-station/health_status"
TIMESTAMP=$(date -Iseconds)

echo -e "${YELLOW}Running health check...${NC}"

# Initialize health status
HEALTH_STATUS="OK"
HEALTH_MESSAGES=()

# Check 1: Service running
echo -e "${YELLOW}Checking service...${NC}"
if systemctl is-active --quiet recording-station; then
    echo -e "${GREEN}✓ Service is running${NC}"
else
    echo -e "${RED}✗ Service is not running${NC}"
    HEALTH_STATUS="ERROR"
    HEALTH_MESSAGES+=("Service not running")
fi

# Check 2: CPU temperature
echo -e "${YELLOW}Checking CPU temperature...${NC}"
if command -v vcgencmd &> /dev/null; then
    TEMP=$(vcgencmd measure_temp | cut -d= -f2 | cut -d\' -f1)
    if (( $(echo "$TEMP > 80" | bc -l) )); then
        echo -e "${RED}✗ CPU temperature too high: ${TEMP}°C${NC}"
        HEALTH_STATUS="WARNING"
        HEALTH_MESSAGES+=("CPU temperature: ${TEMP}°C")
    else
        echo -e "${GREEN}✓ CPU temperature: ${TEMP}°C${NC}"
    fi
fi

# Check 3: Disk space
echo -e "${YELLOW}Checking disk space...${NC}"
DISK_USAGE=$(df -h / | awk 'NR==2 {print $5}' | sed 's/%//')
if [ "$DISK_USAGE" -gt 85 ]; then
    echo -e "${RED}✗ Disk usage critical: ${DISK_USAGE}%${NC}"
    HEALTH_STATUS="WARNING"
    HEALTH_MESSAGES+=("Disk usage: ${DISK_USAGE}%")
elif [ "$DISK_USAGE" -gt 75 ]; then
    echo -e "${YELLOW}⚠ Disk usage high: ${DISK_USAGE}%${NC}"
    HEALTH_STATUS="WARNING"
    HEALTH_MESSAGES+=("Disk usage: ${DISK_USAGE}%")
else
    echo -e "${GREEN}✓ Disk usage: ${DISK_USAGE}%${NC}"
fi

# Check 4: Memory
echo -e "${YELLOW}Checking memory...${NC}"
MEM_USAGE=$(free | awk '/^Mem:/ {printf "%.0f", $3/$2 * 100}')
if [ "$MEM_USAGE" -gt 90 ]; then
    echo -e "${RED}✗ Memory critical: ${MEM_USAGE}%${NC}"
    HEALTH_STATUS="WARNING"
    HEALTH_MESSAGES+=("Memory: ${MEM_USAGE}%")
elif [ "$MEM_USAGE" -gt 80 ]; then
    echo -e "${YELLOW}⚠ Memory high: ${MEM_USAGE}%${NC}"
    HEALTH_STATUS="WARNING"
    HEALTH_MESSAGES+=("Memory: ${MEM_USAGE}%")
else
    echo -e "${GREEN}✓ Memory: ${MEM_USAGE}%${NC}"
fi

# Check 5: Recording directory
echo -e "${YELLOW}Checking recording directory...${NC}"
if [ -d "/var/lib/recording-station/recordings" ]; then
    echo -e "${GREEN}✓ Recording directory exists${NC}"
else
    echo -e "${RED}✗ Recording directory missing${NC}"
    HEALTH_STATUS="ERROR"
    HEALTH_MESSAGES+=("Recording directory missing")
fi

# Check 6: Cloud connection (if enabled)
if [ -f "/var/lib/recording-station/cloud_status" ]; then
    source /var/lib/recording-station/cloud_status
    if [ "$CLOUD_CONNECTED" = "true" ]; then
        echo -e "${GREEN}✓ Cloud connected${NC}"
    else
        echo -e "${YELLOW}⚠ Cloud disconnected${NC}"
        HEALTH_STATUS="WARNING"
        HEALTH_MESSAGES+=("Cloud disconnected")
    fi
fi

# Write health status
echo "HEALTH_STATUS=$HEALTH_STATUS" > "$HEALTH_FILE"
echo "TIMESTAMP=$TIMESTAMP" >> "$HEALTH_FILE"
echo "MESSAGES=${HEALTH_MESSAGES[*]}" >> "$HEALTH_FILE"

# Exit with appropriate code
if [ "$HEALTH_STATUS" = "OK" ]; then
    echo -e "\n${GREEN}✅ System is healthy${NC}"
    exit 0
elif [ "$HEALTH_STATUS" = "WARNING" ]; then
    echo -e "\n${YELLOW}⚠ System has warnings${NC}"
    exit 1
else
    echo -e "\n${RED}❌ System has errors${NC}"
    exit 2
fi
