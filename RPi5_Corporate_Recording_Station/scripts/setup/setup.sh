#!/bin/bash
# setup.sh - Complete system setup for Recording Station
# Run as: sudo ./setup.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Corporate Recording Station Setup              ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Please run as root (sudo ./setup.sh)${NC}"
    exit 1
fi

# Get system information
echo -e "${YELLOW}Detecting system...${NC}"
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    echo -e "${GREEN}✓ Running on ARM64 (Raspberry Pi 5)${NC}"
elif [ "$ARCH" = "armv7l" ] || [ "$ARCH" = "armhf" ]; then
    echo -e "${GREEN}✓ Running on ARMHF${NC}"
else
    echo -e "${YELLOW}⚠ Running on $ARCH (development mode)${NC}"
fi

# Step 1: Update system
echo -e "\n${YELLOW}[1/7] Updating system packages...${NC}"
apt-get update -qq
apt-get upgrade -y -qq

# Step 2: Install dependencies
echo -e "\n${YELLOW}[2/7] Installing dependencies...${NC}"
./install_deps.sh

# Step 3: Configure hardware
echo -e "\n${YELLOW}[3/7] Configuring hardware...${NC}"
./configure_gpio.sh

# Step 4: Setup directories
echo -e "\n${YELLOW}[4/7] Creating directories...${NC}"
mkdir -p /var/lib/recording-station/{recordings,archive,temp,metadata}
mkdir -p /var/log/recording-station
mkdir -p /etc/recording-station
chmod 755 /var/lib/recording-station
chmod 755 /var/log/recording-station

# Step 5: Copy configuration
echo -e "\n${YELLOW}[5/7] Copying configuration...${NC}"
if [ -f "../../config/app/recording-station.conf" ]; then
    cp ../../config/app/recording-station.conf /etc/recording-station/
    cp ../../config/app/logging.conf /etc/recording-station/
    echo -e "${GREEN}✓ Configuration files copied${NC}"
else
    echo -e "${YELLOW}⚠ Configuration files not found, using defaults${NC}"
    # Create default config
    cat > /etc/recording-station/recording-station.conf << EOF
# Recording Station Configuration
DATA_DIR=/var/lib/recording-station
RECORDINGS_DIR=/var/lib/recording-station/recordings
LOG_LEVEL=info
VIDEO_WIDTH=1920
VIDEO_HEIGHT=1080
VIDEO_FRAMERATE=30
AUDIO_SAMPLE_RATE=48000
CLOUD_ENABLED=false
EOF
fi

# Step 6: Build application
echo -e "\n${YELLOW}[6/7] Building application...${NC}"
cd ../../
if [ -f "CMakeLists.txt" ]; then
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    make install
    echo -e "${GREEN}✓ Application built and installed${NC}"
else
    echo -e "${RED}✗ CMakeLists.txt not found${NC}"
    exit 1
fi

# Step 7: Install service
echo -e "\n${YELLOW}[7/7] Installing systemd service...${NC}"
cd ../scripts/deploy/
./install_service.sh

echo -e "\n${GREEN}✅ Setup complete!${NC}"
echo -e "${GREEN}📊 Recording Station is ready${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "🌐 Web Dashboard: http://$(hostname -I | awk '{print $1}'):8080"
echo -e "🔌 UART Console: /dev/ttyAMA0 at 115200 baud"
echo -e "📝 Logs: journalctl -u recording-station -f"
echo -e "🔄 Service: systemctl status recording-station"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
