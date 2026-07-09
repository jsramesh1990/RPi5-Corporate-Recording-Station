#!/bin/bash
# install_deps.sh - Install all dependencies for Recording Station

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Installing dependencies...${NC}"

# Essential packages
ESSENTIAL_PKGS=(
    build-essential
    cmake
    git
    pkg-config
    curl
    wget
    sudo
)

# Development packages
DEV_PKGS=(
    g++
    gcc
    make
    autoconf
    automake
    libtool
)

# Libraries
LIB_PKGS=(
    libboost-all-dev
    libsqlite3-dev
    libcurl4-openssl-dev
    libjsoncpp-dev
    libwebsocketpp-dev
    libssl-dev
    libavcodec-dev
    libavformat-dev
    libavutil-dev
    libswscale-dev
    libavresample-dev
    libv4l-dev
    libasound2-dev
    libpci-dev
    libi2c-dev
    libspi-dev
)

# Tools
TOOL_PKGS=(
    v4l-utils
    alsa-utils
    i2c-tools
    spi-tools
    htop
    iotop
    stress
    fio
    screen
    minicom
    tmux
)

echo -e "${YELLOW}Updating package list...${NC}"
apt-get update -qq

echo -e "${YELLOW}Installing essential packages...${NC}"
apt-get install -y -qq "${ESSENTIAL_PKGS[@]}"

echo -e "${YELLOW}Installing development packages...${NC}"
apt-get install -y -qq "${DEV_PKGS[@]}"

echo -e "${YELLOW}Installing libraries...${NC}"
apt-get install -y -qq "${LIB_PKGS[@]}"

echo -e "${YELLOW}Installing tools...${NC}"
apt-get install -y -qq "${TOOL_PKGS[@]}"

# Install ffmpeg if not available
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${YELLOW}Installing ffmpeg...${NC}"
    apt-get install -y -qq ffmpeg
fi

# Install Python dependencies for web interface
if command -v pip3 &> /dev/null; then
    echo -e "${YELLOW}Installing Python dependencies...${NC}"
    pip3 install --quiet flask flask-socketio flask-cors
fi

# Verify installations
echo -e "\n${GREEN}Installed packages:${NC}"
for pkg in g++ cmake git ffmpeg; do
    if command -v $pkg &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} $pkg"
    else
        echo -e "  ${RED}✗${NC} $pkg"
    fi
done

echo -e "\n${GREEN}✅ Dependencies installed successfully${NC}"
