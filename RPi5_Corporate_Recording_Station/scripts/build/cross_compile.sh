#!/bin/bash
# cross_compile.sh - Cross-compile for Raspberry Pi 5 on x86_64

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RPi5 Corporate Recording Station Cross-Compile      ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Check if cross-compiler is installed
if ! command -v aarch64-linux-gnu-g++ &> /dev/null; then
    echo -e "${RED}❌ Cross-compiler not found. Installing...${NC}"
    sudo apt-get update
    sudo apt-get install -y g++-aarch64-linux-gnu binutils-aarch64-linux-gnu
fi

# Check for Raspberry Pi sysroot
RPI_SYSROOT=${RPI_SYSROOT:-/usr/aarch64-linux-gnu}
if [ ! -d "$RPI_SYSROOT" ]; then
    echo -e "${YELLOW}⚠ Sysroot not found at $RPI_SYSROOT${NC}"
    echo -e "${YELLOW}  Set RPI_SYSROOT environment variable to point to Raspberry Pi rootfs${NC}"
    echo -e "${YELLOW}  Or continue with default sysroot${NC}"
    read -p "Continue? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Create build directory
mkdir -p build-cross
cd build-cross

# Configure with cross-compilation
echo -e "${YELLOW}Configuring CMake for cross-compilation...${NC}"
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/aarch64-linux-gnu.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

# Check if build succeeded
if [ -f "bin/recording-station" ] || [ -f "recording-station" ]; then
    echo -e "${GREEN}✅ Cross-compilation successful!${NC}"
    
    # Show binary info
    if [ -f "bin/recording-station" ]; then
        BINARY="bin/recording-station"
    else
        BINARY="recording-station"
    fi
    
    echo -e "\n${YELLOW}Binary information:${NC}"
    file $BINARY
    ls -lh $BINARY
    
    echo -e "\n${GREEN}📦 Binary is ready for Raspberry Pi 5${NC}"
else
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi
