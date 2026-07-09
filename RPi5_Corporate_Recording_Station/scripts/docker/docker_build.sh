#!/bin/bash
# docker_build.sh - Build Docker image for Recording Station

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Recording Station Docker Build                 ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo -e "${RED}❌ Docker not found. Install: curl -fsSL https://get.docker.com | sh${NC}"
    exit 1
fi

# Check if running on ARM64 (Raspberry Pi 5)
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    echo -e "${GREEN}✓ Building for ARM64${NC}"
    PLATFORM="linux/arm64"
elif [ "$ARCH" = "x86_64" ]; then
    echo -e "${YELLOW}⚠ Building for x86_64 (emulation)${NC}"
    PLATFORM="linux/amd64"
else
    echo -e "${RED}❌ Unsupported architecture: $ARCH${NC}"
    exit 1
fi

# Build Docker image
echo -e "\n${YELLOW}Building Docker image...${NC}"

docker build \
    --platform $PLATFORM \
    --tag recording-station:latest \
    --build-arg BUILD_TYPE=Release \
    --file Dockerfile \
    .

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Docker image built successfully${NC}"
    
    # Show image info
    echo -e "\n${YELLOW}Image information:${NC}"
    docker images recording-station
    
    echo -e "\n${GREEN}📦 Image: recording-station:latest${NC}"
    echo -e "${YELLOW}To run: ./docker_run.sh${NC}"
else
    echo -e "${RED}❌ Docker build failed${NC}"
    exit 1
fi
