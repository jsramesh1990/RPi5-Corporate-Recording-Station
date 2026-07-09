#!/bin/bash
# deploy.sh - Deploy Recording Station to Raspberry Pi

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Corporate Recording Station Deploy             ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Check parameters
if [ -z "$1" ]; then
    echo -e "${YELLOW}Usage: ./deploy.sh <raspberry-pi-ip> [username]${NC}"
    echo -e "Example: ./deploy.sh 192.168.1.100 pi"
    exit 1
fi

PI_IP=$1
PI_USER=${2:-pi}
PI_HOST="$PI_USER@$PI_IP"

echo -e "${YELLOW}Deploying to: $PI_HOST${NC}"

# Build with cross-compilation
echo -e "\n${YELLOW}[1/5] Building...${NC}"
cd ../build/
./cross_compile.sh

# Copy binary
echo -e "\n${YELLOW}[2/5] Copying binary...${NC}"
scp bin/recording-station $PI_HOST:/home/$PI_USER/

# Copy configuration
echo -e "\n${YELLOW}[3/5] Copying configuration...${NC}"
scp -r ../config/* $PI_HOST:/home/$PI_USER/config/

# Copy web interface
echo -e "\n${YELLOW}[4/5] Copying web interface...${NC}"
scp -r ../web/* $PI_HOST:/home/$PI_USER/web/

# Install on Raspberry Pi
echo -e "\n${YELLOW}[5/5] Installing on Raspberry Pi...${NC}"
ssh $PI_HOST "sudo mkdir -p /opt/recording-station"
ssh $PI_HOST "sudo cp /home/$PI_USER/recording-station /opt/recording-station/"
ssh $PI_HOST "sudo chmod +x /opt/recording-station/recording-station"
ssh $PI_HOST "sudo cp -r /home/$PI_USER/config /etc/recording-station"
ssh $PI_HOST "sudo cp -r /home/$PI_USER/web /var/www/recording-station"

# Install service
echo -e "\n${YELLOW}Installing service...${NC}"
ssh $PI_HOST "sudo /home/$PI_USER/scripts/deploy/install_service.sh"

echo -e "\n${GREEN}✅ Deployment complete!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "🌐 Web Dashboard: http://$PI_IP:8080"
echo -e "🔌 UART Console: screen /dev/ttyAMA0 115200"
echo -e "📝 Logs: ssh $PI_HOST 'sudo journalctl -u recording-station -f'"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
