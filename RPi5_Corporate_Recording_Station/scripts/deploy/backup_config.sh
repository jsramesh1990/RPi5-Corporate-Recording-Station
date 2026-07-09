#!/bin/bash
# backup_config.sh - Backup configuration files

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Recording Station Config Backup                ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

BACKUP_DIR="/var/backups/recording-station"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_PATH="$BACKUP_DIR/backup_$TIMESTAMP"

echo -e "${YELLOW}Creating backup at: $BACKUP_PATH${NC}"

# Create backup directory
mkdir -p "$BACKUP_PATH"

# Backup configuration
echo -e "${YELLOW}Backing up configuration...${NC}"
if [ -d "/etc/recording-station" ]; then
    cp -r /etc/recording-station "$BACKUP_PATH/config"
    echo -e "${GREEN}✓ Configuration backed up${NC}"
else
    echo -e "${YELLOW}⚠ No configuration found${NC}"
fi

# Backup systemd services
echo -e "${YELLOW}Backing up systemd services...${NC}"
mkdir -p "$BACKUP_PATH/services"
for service in recording-station recording-station-monitor recording-station-watchdog; do
    if [ -f "/etc/systemd/system/$service.service" ]; then
        cp "/etc/systemd/system/$service.service" "$BACKUP_PATH/services/"
        echo -e "${GREEN}✓ $service.service backed up${NC}"
    fi
done

# Backup GPIO configuration
echo -e "${YELLOW}Backing up GPIO configuration...${NC}"
if [ -f "/etc/udev/rules.d/99-gpio.rules" ]; then
    cp "/etc/udev/rules.d/99-gpio.rules" "$BACKUP_PATH/"
    echo -e "${GREEN}✓ GPIO rules backed up${NC}"
fi

# Backup boot configuration
echo -e "${YELLOW}Backing up boot configuration...${NC}"
if [ -f "/boot/firmware/config.txt" ]; then
    cp "/boot/firmware/config.txt" "$BACKUP_PATH/config.txt"
    echo -e "${GREEN}✓ config.txt backed up${NC}"
fi
if [ -f "/boot/firmware/cmdline.txt" ]; then
    cp "/boot/firmware/cmdline.txt" "$BACKUP_PATH/cmdline.txt"
    echo -e "${GREEN}✓ cmdline.txt backed up${NC}"
fi

# Create backup info
cat > "$BACKUP_PATH/backup_info.txt" << EOF
Backup Information
==================
Timestamp: $TIMESTAMP
Hostname: $(hostname)
Kernel: $(uname -r)
System: $(lsb_release -ds 2>/dev/null || echo "Unknown")

Files backed up:
$(find "$BACKUP_PATH" -type f | sed 's/^/  /')
EOF

# Create archive
echo -e "${YELLOW}Creating archive...${NC}"
cd "$BACKUP_DIR"
tar -czf "backup_$TIMESTAMP.tar.gz" "backup_$TIMESTAMP"
rm -rf "backup_$TIMESTAMP"

echo -e "${GREEN}✅ Backup complete!${NC}"
echo -e "${GREEN}📦 Archive: $BACKUP_DIR/backup_$TIMESTAMP.tar.gz${NC}"
echo -e "${GREEN}📄 Size: $(du -h "$BACKUP_DIR/backup_$TIMESTAMP.tar.gz" | cut -f1)${NC}"

# List existing backups
echo -e "\n${YELLOW}Existing backups:${NC}"
ls -lh "$BACKUP_DIR"/*.tar.gz 2>/dev/null | tail -5 || echo "No existing backups"
