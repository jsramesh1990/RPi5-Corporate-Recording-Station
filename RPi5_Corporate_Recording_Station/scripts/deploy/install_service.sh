#!/bin/bash
# install_service.sh - Install systemd service for Recording Station

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Installing systemd service...${NC}"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Please run as root (sudo ./install_service.sh)${NC}"
    exit 1
fi

# Create service file
cat > /etc/systemd/system/recording-station.service << 'EOF'
[Unit]
Description=RPi5 Corporate Recording Station
Documentation=https://github.com/yourusername/rpi5-corporate-recording-station
After=network.target sound.target multi-user.target
Wants=network.target

[Service]
Type=simple
User=root
Group=root
WorkingDirectory=/opt/recording-station
ExecStart=/opt/recording-station/recording-station
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier=recording-station
EnvironmentFile=/etc/recording-station/recording-station.conf

# Security hardening
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/var/lib/recording-station /var/log/recording-station /var/tmp

[Install]
WantedBy=multi-user.target
EOF

# Create monitor service
cat > /etc/systemd/system/recording-station-monitor.service << 'EOF'
[Unit]
Description=RPi5 Recording Station Monitor
After=recording-station.service

[Service]
Type=simple
User=root
WorkingDirectory=/opt/recording-station
ExecStart=/usr/local/bin/storage-monitor -r -i 10
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Create watchdog service
cat > /etc/systemd/system/recording-station-watchdog.service << 'EOF'
[Unit]
Description=RPi5 Recording Station Watchdog
After=recording-station.service
Requires=recording-station.service

[Service]
Type=simple
User=root
WorkingDirectory=/opt/recording-station
ExecStart=/usr/local/bin/health_check.sh
Restart=always
RestartSec=30

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd
systemctl daemon-reload

# Enable services
systemctl enable recording-station
systemctl enable recording-station-monitor
systemctl enable recording-station-watchdog

# Start services
systemctl start recording-station
systemctl start recording-station-monitor
systemctl start recording-station-watchdog

echo -e "${GREEN}✅ Services installed and started${NC}"
echo -e "\n${YELLOW}Service status:${NC}"
systemctl status recording-station --no-pager

echo -e "\n${GREEN}📊 Services:${NC}"
echo -e "  recording-station         - Main application"
echo -e "  recording-station-monitor - Storage monitor"
echo -e "  recording-station-watchdog - Health watchdog"
echo -e "\n${YELLOW}Logs: journalctl -u recording-station -f${NC}"
