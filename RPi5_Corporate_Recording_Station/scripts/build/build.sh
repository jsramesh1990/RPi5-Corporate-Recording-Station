#!/bin/bash
# build.sh - Build Recording Station application

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Corporate Recording Station Build              ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Get build type
BUILD_TYPE=${1:-Release}
echo -e "${YELLOW}Build type: $BUILD_TYPE${NC}"

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    echo -e "${GREEN}✓ Building for ARM64 (native)${NC}"
    CMAKE_OPTS=""
elif [ "$ARCH" = "x86_64" ]; then
    echo -e "${YELLOW}⚠ Building for x86_64 (development)${NC}"
    CMAKE_OPTS=""
else
    echo -e "${YELLOW}⚠ Building for $ARCH${NC}"
    CMAKE_OPTS=""
fi

# Create build directory
mkdir -p build
cd build

# Configure
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    $CMAKE_OPTS

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

# Check if build succeeded
if [ -f "bin/recording-station" ] || [ -f "recording-station" ]; then
    echo -e "${GREEN}✅ Build successful!${NC}"
    
    # Show binary info
    if [ -f "bin/recording-station" ]; then
        BINARY="bin/recording-station"
    else
        BINARY="recording-station"
    fi
    
    echo -e "\n${YELLOW}Binary information:${NC}"
    file $BINARY
    ls -lh $BINARY
    
    # Install if requested
    if [ "$2" = "--install" ]; then
        echo -e "${YELLOW}Installing...${NC}"
        make install
        echo -e "${GREEN}✅ Installation complete${NC}"
    fi
else
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi
