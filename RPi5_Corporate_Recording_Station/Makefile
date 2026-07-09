# Makefile for RPi5 Corporate Recording Station
# Target: ARM64 (Raspberry Pi 5)

.PHONY: all clean debug release install uninstall test doc

# Compiler settings
CXX = aarch64-linux-gnu-g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2 -march=armv8.2-a+crypto -mtune=cortex-a76
LDFLAGS = -pthread -ljsoncpp -lcurl -lssl -lcrypto -lavcodec -lavformat -lavutil -lswscale -lwebsockets

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

# Source files
SOURCES = $(shell find $(SRC_DIR) -name "*.cpp")
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Main target
all: $(BIN_DIR)/recording-station

# Create directories
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Link
$(BIN_DIR)/recording-station: $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Debug build
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: $(BIN_DIR)/recording-station

# Release build
release: CXXFLAGS += -O3 -DNDEBUG
release: $(BIN_DIR)/recording-station

# Clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install
install: $(BIN_DIR)/recording-station
	sudo cp $(BIN_DIR)/recording-station /usr/local/bin/
	sudo cp -r config /etc/recording-station
	sudo cp -r web /var/www/recording-station
	sudo systemctl daemon-reload
	sudo systemctl enable recording-station
	sudo systemctl start recording-station

# Uninstall
uninstall:
	sudo systemctl stop recording-station
	sudo systemctl disable recording-station
	sudo rm -f /usr/local/bin/recording-station
	sudo rm -rf /etc/recording-station
	sudo rm -rf /var/www/recording-station

# Tests
test: $(BIN_DIR)/recording-station
	cd tests && make test

# Documentation
doc:
	doxygen Doxyfile

# Format
format:
	find $(SRC_DIR) $(INCLUDE_DIR) -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" | xargs clang-format -i

# Static analysis
analyze:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)

# Help
help:
	@echo "Available targets:"
	@echo "  all         - Build the application (default)"
	@echo "  debug       - Build with debug symbols"
	@echo "  release     - Build with optimizations"
	@echo "  clean       - Remove build artifacts"
	@echo "  install     - Install system-wide"
	@echo "  uninstall   - Remove installation"
	@echo "  test        - Run tests"
	@echo "  doc         - Generate documentation"
	@echo "  format      - Format code"
	@echo "  analyze     - Static analysis"
