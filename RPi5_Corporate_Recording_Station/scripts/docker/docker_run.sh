#!/bin/bash
# docker_run.sh - Run Recording Station in Docker container

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Recording Station Docker Run                   ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Check if image exists
if ! docker images recording-station:latest &> /dev/null; then
    echo -e "${YELLOW}⚠ Image not found. Building first...${NC}"
    ./docker_build.sh
fi

# Check if container is already running
if docker ps -a | grep -q recording-station; then
    echo -e "${YELLOW}Container already exists. Removing...${NC}"
    docker rm -f recording-station 2>/dev/null || true
fi

# Create directories if they don't exist
mkdir -p /var/lib/recording-station/{recordings,archive,temp,metadata}
mkdir -p /var/log/recording-station

echo -e "\n${YELLOW}Starting Docker container...${NC}"

# Run container with privileged access for GPIO
docker run -d \
    --name recording-station \
    --restart unless-stopped \
    --privileged \
    --network host \
    --device /dev/ttyAMA0 \
    --device /dev/video0 \
    --device /dev/snd \
    -v /var/lib/recording-station:/var/lib/recording-station \
    -v /var/log/recording-station:/var/log/recording-station \
    -v /etc/recording-station:/etc/recording-station:ro \
    -v /sys:/sys \
    -v /dev:/dev \
    -e TZ=UTC \
    -e LOG_LEVEL=info \
    recording-station:latest

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Container started successfully${NC}"
    
    echo -e "\n${YELLOW}Container status:${NC}"
    docker ps | grep recording-station
    
    echo -e "\n${GREEN}📊 Container is running${NC}"
    echo -e "🌐 Web Dashboard: http://$(hostname -I | awk '{print $1}'):8080"
    echo -e "📝 Logs: docker logs -f recording-station"
    echo -e "🛑 Stop: docker stop recording-station"
else
    echo -e "${RED}❌ Container failed to start${NC}"
    exit 1
fi
