#include "GPIOController.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

// I2C headers
#include <linux/i2c-dev.h>

// SPI headers
#include <linux/spi/spidev.h>

// ============================================
// SINGLETON
// ============================================

GPIOController& GPIOController::getInstance() {
    static GPIOController instance;
    return instance;
}

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

GPIOController::GPIOController() {
    // Initialize pin configs with default values
    for (int i = 0; i < MAX_PINS; i++) {
        PinConfig config;
        config.pin_number = i;
        config.name = "GPIO" + std::to_string(i);
        config.direction = PinDirection::INPUT;
        config.pull = PullMode::NONE;
        config.drive = DriveStrength::DRIVE_8MA;
        config.interrupt_enabled = false;
        config.interrupt_mode = InterruptMode::EDGE_NONE;
        config.initial_value = 0;
        config.invert = false;
        config.debounce_ms = 0;
        pin_configs[i] = config;
        
        PinInfo info;
        info.pin = i;
        info.name = "GPIO" + std::to_string(i);
        info.direction = PinDirection::INPUT;
        info.pull = PullMode::NONE;
        info.value = false;
        info.interrupt_enabled = false;
        info.interrupt_mode = InterruptMode::EDGE_NONE;
        info.last_change_time = 0;
        info.change_count = 0;
        pin_info[i] = info;
    }
}

GPIOController::~GPIOController() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool GPIOController::initialize() {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (initialized) {
        return true;
    }
    
    // Map GPIO registers
    gpio_memory = mapPeripheral(GPIO_BASE, GPIO_SIZE);
    if (gpio_memory == nullptr) {
        setError("Failed to map GPIO registers");
        return false;
    }
    
    regs = static_cast<GPIORegisters*>(gpio_memory);
    
    // Start interrupt processing thread
    interrupt_running = true;
    std::thread interrupt_thread(&GPIOController::processInterrupts, this);
    interrupt_thread.detach();
    
    initialized = true;
    std::cout << "GPIO Controller initialized" << std::endl;
    
    return true;
}

void GPIOController::shutdown() {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (!initialized) {
        return;
    }
    
    // Stop interrupt processing
    interrupt_running = false;
    
    // Disable all interrupts
    for (auto& [pin, _] : interrupt_callbacks) {
        disableInterrupt(pin);
    }
    
    // Unmap registers
    unmapPeripheral();
    regs = nullptr;
    gpio_memory = nullptr;
    
    initialized = false;
    std::cout << "GPIO Controller shutdown" << std::endl;
}

void* GPIOController::mapPeripheral(uint32_t base, size_t size) {
    int mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        setError("Failed to open /dev/gpiomem");
        return nullptr;
    }
    
    void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                        MAP_SHARED, mem_fd, base);
    close(mem_fd);
    
    if (mapped == MAP_FAILED) {
        setError("Failed to map memory");
        return nullptr;
    }
    
    return mapped;
}

void GPIOController::unmapPeripheral() {
    if (gpio_memory != nullptr) {
        munmap(gpio_memory, GPIO_SIZE);
        gpio_memory = nullptr;
    }
}

// ============================================
// PIN CONFIGURATION
// ============================================

bool GPIOController::configurePin(const PinConfig& config) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (!initialized) {
        setError("GPIO not initialized");
        return false;
    }
    
    if (!isPinValid(config.pin_number)) {
        setError("Invalid pin number: " + std::to_string(config.pin_number));
        return false;
    }
    
    // Store configuration
    pin_configs[config.pin_number] = config;
    
    // Set direction/function
    int function = static_cast<int>(config.direction);
    setPinFunction(config.pin_number, function);
    
    // Set pull mode
    int pull = static_cast<int>(config.pull);
    setPullMode(config.pin_number, pull);
    
    // Set initial value if output
    if (config.direction == PinDirection::OUTPUT) {
        writePin(config.pin_number, config.initial_value != 0);
    }
    
    // Update pin info
    updatePinInfo(config.pin_number);
    
    return true;
}

bool GPIOController::configurePin(int pin, PinDirection direction, int initial_value) {
    PinConfig config;
    config.pin_number = pin;
    config.direction = direction;
    config.initial_value = initial_value;
    return configurePin(config);
}

GPIOController::PinConfig GPIOController::getPinConfig(int pin) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto it = pin_configs.find(pin);
    if (it != pin_configs.end()) {
        return it->second;
    }
    return PinConfig{};
}

GPIOController::PinInfo GPIOController::getPinInfo(int pin) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto it = pin_info.find(pin);
    if (it != pin_info.end()) {
        return it->second;
    }
    return PinInfo{};
}

std::vector<GPIOController::PinInfo> GPIOController::getAllPinInfo() const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    std::vector<PinInfo> result;
    for (const auto& [pin, info] : pin_info) {
        result.push_back(info);
    }
    return result;
}

bool GPIOController::setPinName(int pin, const std::string& name) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    if (!isPinValid(pin)) {
        return false;
    }
    pin_configs[pin].name = name;
    pin_info[pin].name = name;
    return true;
}

std::string GPIOController::getPinName(int pin) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto it = pin_configs.find(pin);
    if (it != pin_configs.end()) {
        return it->second.name;
    }
    return "GPIO" + std::to_string(pin);
}

int GPIOController::getPinByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    for (const auto& [pin, config] : pin_configs) {
        if (config.name == name) {
            return pin;
        }
    }
    return -1;
}

// ============================================
// DIGITAL I/O
// ============================================

bool GPIOController::writePin(int pin, bool value) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (!initialized || !isPinValid(pin)) {
        return false;
    }
    
    // Check if pin is output
    auto it = pin_configs.find(pin);
    if (it == pin_configs.end() || it->second.direction != PinDirection::OUTPUT) {
        setError("Pin " + std::to_string(pin) + " is not configured as output");
        return false;
    }
    
    // Handle inversion
    if (it->second.invert) {
        value = !value;
    }
    
    int reg_index = pin / 32;
    int bit = pin % 32;
    
    if (value) {
        regs->GPSET[reg_index] = 1 << bit;
    } else {
        regs->GPCLR[reg_index] = 1 << bit;
    }
    
    // Update pin info
    updatePinInfo(pin);
    
    return true;
}

bool GPIOController::writePin(int pin, PinState state) {
    switch (state) {
        case PinState::HIGH:
            return writePin(pin, true);
        case PinState::LOW:
            return writePin(pin, false);
        case PinState::TOGGLE:
            return togglePin(pin);
        default:
            return false;
    }
}

bool GPIOController::readPin(int pin) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (!initialized || !isPinValid(pin)) {
        return false;
    }
    
    int reg_index = pin / 32;
    int bit = pin % 32;
    bool value = (regs->GPLEV[reg_index] & (1 << bit)) != 0;
    
    // Handle inversion
    auto it = pin_configs.find(pin);
    if (it != pin_configs.end() && it->second.invert) {
        value = !value;
    }
    
    return value;
}

bool GPIOController::togglePin(int pin) {
    bool current = readPin(pin);
    return writePin(pin, !current);
}

int GPIOController::readPinValue(int pin) const {
    return readPin(pin) ? 1 : 0;
}

size_t GPIOController::writePins(const std::vector<int>& pins, const std::vector<bool>& values) {
    if (pins.size() != values.size()) {
        return 0;
    }
    
    size_t success = 0;
    for (size_t i = 0; i < pins.size(); i++) {
        if (writePin(pins[i], values[i])) {
            success++;
        }
    }
    return success;
}

std::map<int, bool> GPIOController::readPins(const std::vector<int>& pins) const {
    std::map<int, bool> result;
    for (int pin : pins) {
        result[pin] = readPin(pin);
    }
    return result;
}

bool GPIOController::setPinGroup(const std::vector<int>& pins, uint32_t value) {
    for (size_t i = 0; i < pins.size() && i < 32; i++) {
        if (!writePin(pins[i], (value & (1 << i)) != 0)) {
            return false;
        }
    }
    return true;
}

uint32_t GPIOController::getPinGroup(const std::vector<int>& pins) const {
    uint32_t result = 0;
    for (size_t i = 0; i < pins.size() && i < 32; i++) {
        if (readPin(pins[i])) {
            result |= (1 << i);
        }
    }
    return result;
}

// ============================================
// PWM
// ============================================

bool GPIOController::enablePWM(int pin, int frequency, int duty_cycle) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (!initialized || !isPinValid(pin)) {
        return false;
    }
    
    if (!isPinPWM(pin)) {
        setError("Pin " + std::to_string(pin) + " does not support PWM");
        return false;
    }
    
    PWMConfig config;
    config.pin = pin;
    config.frequency = frequency;
    config.duty_cycle = std::max(0, std::min(100, duty_cycle));
    config.enabled = true;
    config.range = 1024;
    config.clock_divisor = 1;
    
    pwm_configs[pin] = config;
    
    // Set pin to ALT function for PWM
    setPinFunction(pin, 5); // ALT5 for PWM
    
    // Setup PWM hardware
    if (!setupPWM(pin)) {
        pwm_configs[pin].enabled = false;
        return false;
    }
    
    // Update PWM
    updatePWM(pin);
    
    return true;
}

bool GPIOController::disablePWM(int pin) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    auto it = pwm_configs.find(pin);
    if (it == pwm_configs.end()) {
        return false;
    }
    
    it->second.enabled = false;
    pwm_configs.erase(it);
    
    // Reset pin function
    setPinFunction(pin, 0); // GPIO input
    
    return true;
}

bool GPIOController::setPWMFrequency(int pin, int frequency) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    auto it = pwm_configs.find(pin);
    if (it == pwm_configs.end() || !it->second.enabled) {
        return false;
    }
    
    it->second.frequency = frequency;
    updatePWM(pin);
    return true;
}

bool GPIOController::setPWMDutyCycle(int pin, int duty_cycle) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    auto it = pwm_configs.find(pin);
    if (it == pwm_configs.end() || !it->second.enabled) {
        return false;
    }
    
    it->second.duty_cycle = std::max(0, std::min(100, duty_cycle));
    updatePWM(pin);
    return true;
}

GPIOController::PWMConfig GPIOController::getPWMConfig(int pin) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto it = pwm_configs.find(pin);
    if (it != pwm_configs.end()) {
        return it->second;
    }
    return PWMConfig{};
}

bool GPIOController::isPWMEnabled(int pin) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto it = pwm_configs.find(pin);
    return it != pwm_configs.end() && it->second.enabled;
}

bool GPIOController::setupPWM(int pin) {
    // This is a simplified PWM implementation
    // In production, would use hardware PWM or software PWM with timer
    // For now, we simulate with software PWM using a separate thread
    return true;
}

void GPIOController::updatePWM(int pin) {
    // Update PWM output
    // In production, would update hardware PWM registers
    // For now, we simulate by toggling with software
}

// ============================================
// INTERRUPTS
// ============================================

bool GPIOController::enableInterrupt(int pin, InterruptCallback callback, InterruptMode mode) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (!initialized || !isPinValid(pin)) {
        return false;
    }
    
    // Store callback
    interrupt_callbacks[pin] = callback;
    pin_configs[pin].interrupt_enabled = true;
    pin_configs[pin].interrupt_mode = mode;
    
    // Setup interrupt hardware
    setupInterrupt(pin, mode);
    
    // Update pin info
    updatePinInfo(pin);
    
    return true;
}

bool GPIOController::enableEdgeInterrupt(int pin, EdgeCallback callback, bool rising) {
    auto wrapper = [callback, rising](int pin, bool value) {
        if (callback) {
            callback(pin, rising);
        }
    };
    
    InterruptMode mode = rising ? InterruptMode::EDGE_RISING : InterruptMode::EDGE_FALLING;
    return enableInterrupt(pin, wrapper, mode);
}

bool GPIOController::disableInterrupt(int pin) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (!isPinValid(pin)) {
        return false;
    }
    
    // Clear interrupt registers
    int reg_index = pin / 32;
    int bit = pin % 32;
    
    regs->GPREN[reg_index] &= ~(1 << bit);
    regs->GPFEN[reg_index] &= ~(1 << bit);
    regs->GPHEN[reg_index] &= ~(1 << bit);
    regs->GPLEN[reg_index] &= ~(1 << bit);
    regs->GPAREN[reg_index] &= ~(1 << bit);
    regs->GPAFEN[reg_index] &= ~(1 << bit);
    
    // Clear pending interrupts
    regs->GPEDS[reg_index] = (1 << bit);
    
    // Remove callbacks
    interrupt_callbacks.erase(pin);
    edge_callbacks.erase(pin);
    pin_configs[pin].interrupt_enabled = false;
    
    // Update pin info
    updatePinInfo(pin);
    
    return true;
}

bool GPIOController::isInterruptEnabled(int pin) const {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto it = pin_configs.find(pin);
    return it != pin_configs.end() && it->second.interrupt_enabled;
}

void GPIOController::clearInterrupt(int pin) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    if (isPinValid(pin)) {
        int reg_index = pin / 32;
        int bit = pin % 32;
        regs->GPEDS[reg_index] = (1 << bit);
    }
}

bool GPIOController::waitForInterrupt(int pin, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    uint64_t last_value = readPin(pin) ? 1 : 0;
    
    while (true) {
        uint64_t current_value = readPin(pin) ? 1 : 0;
        if (current_value != last_value) {
            return true;
        }
        
        if (timeout_ms >= 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_ms) {
                return false;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void GPIOController::setupInterrupt(int pin, InterruptMode mode) {
    int reg_index = pin / 32;
    int bit = pin % 32;
    
    // Clear existing interrupts
    regs->GPREN[reg_index] &= ~(1 << bit);
    regs->GPFEN[reg_index] &= ~(1 << bit);
    regs->GPHEN[reg_index] &= ~(1 << bit);
    regs->GPLEN[reg_index] &= ~(1 << bit);
    regs->GPAREN[reg_index] &= ~(1 << bit);
    regs->GPAFEN[reg_index] &= ~(1 << bit);
    
    // Set interrupt mode
    switch (mode) {
        case InterruptMode::EDGE_RISING:
            regs->GPREN[reg_index] |= (1 << bit);
            break;
        case InterruptMode::EDGE_FALLING:
            regs->GPFEN[reg_index] |= (1 << bit);
            break;
        case InterruptMode::EDGE_BOTH:
            regs->GPREN[reg_index] |= (1 << bit);
            regs->GPFEN[reg_index] |= (1 << bit);
            break;
        case InterruptMode::LEVEL_HIGH:
            regs->GPHEN[reg_index] |= (1 << bit);
            break;
        case InterruptMode::LEVEL_LOW:
            regs->GPLEN[reg_index] |= (1 << bit);
            break;
        default:
            break;
    }
}

void GPIOController::processInterrupts() {
    while (interrupt_running) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        
        std::lock_guard<std::mutex> lock(interrupt_mutex);
        
        // Check all pins for interrupts
        for (auto& [pin, callback] : interrupt_callbacks) {
            int reg_index = pin / 32;
            int bit = pin % 32;
            
            // Check if interrupt is pending
            if (regs->GPEDS[reg_index] & (1 << bit)) {
                // Clear the interrupt
                regs->GPEDS[reg_index] = (1 << bit);
                
                // Read the pin value
                bool value = readPin(pin);
                
                // Call the callback
                if (callback) {
                    callback(pin, value);
                }
                
                // Update pin info
                updatePinInfo(pin);
            }
        }
    }
}

void GPIOController::interruptHandler(int pin) {
    // Called when interrupt occurs
    // The actual processing is done in processInterrupts
}

// ============================================
// I2C INTERFACE
// ============================================

bool GPIOController::i2cInit(int bus, int speed) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    std::string device = "/dev/i2c-" + std::to_string(bus);
    int fd = open(device.c_str(), O_RDWR);
    if (fd < 0) {
        setError("Failed to open I2C device: " + device);
        return false;
    }
    
    // Set speed
    if (ioctl(fd, I2C_SPEED, speed) < 0) {
        close(fd);
        setError("Failed to set I2C speed");
        return false;
    }
    
    I2CInterface iface;
    iface.bus = bus;
    iface.speed = speed;
    iface.fd = fd;
    i2c_interfaces[bus] = iface;
    
    return true;
}

bool GPIOController::i2cWrite(int bus, uint8_t address, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    auto it = i2c_interfaces.find(bus);
    if (it == i2c_interfaces.end()) {
        setError("I2C bus not initialized: " + std::to_string(bus));
        return false;
    }
    
    if (ioctl(it->second.fd, I2C_SLAVE, address) < 0) {
        setError("Failed to set I2C address: 0x" + std::to_string(address));
        return false;
    }
    
    if (write(it->second.fd, data.data(), data.size()) != (ssize_t)data.size()) {
        setError("Failed to write I2C data");
        return false;
    }
    
    return true;
}

std::vector<uint8_t> GPIOController::i2cRead(int bus, uint8_t address, size_t length) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    std::vector<uint8_t> result;
    
    auto it = i2c_interfaces.find(bus);
    if (it == i2c_interfaces.end()) {
        setError("I2C bus not initialized: " + std::to_string(bus));
        return result;
    }
    
    if (ioctl(it->second.fd, I2C_SLAVE, address) < 0) {
        setError("Failed to set I2C address: 0x" + std::to_string(address));
        return result;
    }
    
    result.resize(length);
    if (read(it->second.fd, result.data(), length) != (ssize_t)length) {
        setError("Failed to read I2C data");
        result.clear();
    }
    
    return result;
}

bool GPIOController::i2cWriteRead(int bus, uint8_t address,
                                 const std::vector<uint8_t>& writeData,
                                 std::vector<uint8_t>& readData,
                                 size_t readLength) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    auto it = i2c_interfaces.find(bus);
    if (it == i2c_interfaces.end()) {
        setError("I2C bus not initialized: " + std::to_string(bus));
        return false;
    }
    
    if (ioctl(it->second.fd, I2C_SLAVE, address) < 0) {
        setError("Failed to set I2C address: 0x" + std::to_string(address));
        return false;
    }
    
    // Write
    if (!writeData.empty()) {
        if (write(it->second.fd, writeData.data(), writeData.size()) != (ssize_t)writeData.size()) {
            setError("Failed to write I2C data");
            return false;
        }
    }
    
    // Read
    readData.resize(readLength);
    if (read(it->second.fd, readData.data(), readLength) != (ssize_t)readLength) {
        setError("Failed to read I2C data");
        return false;
    }
    
    return true;
}

std::vector<uint8_t> GPIOController::i2cScan(int bus) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    std::vector<uint8_t> devices;
    
    auto it = i2c_interfaces.find(bus);
    if (it == i2c_interfaces.end()) {
        if (!i2cInit(bus)) {
            return devices;
        }
        it = i2c_interfaces.find(bus);
    }
    
    for (int addr = 0x03; addr < 0x78; addr++) {
        if (ioctl(it->second.fd, I2C_SLAVE, addr) < 0) {
            continue;
        }
        
        // Try to read 1 byte
        uint8_t test;
        if (read(it->second.fd, &test, 1) == 1) {
            devices.push_back(addr);
        }
    }
    
    return devices;
}

// ============================================
// SPI INTERFACE
// ============================================

bool GPIOController::spiInit(int bus, int speed, int mode) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    std::string device = "/dev/spidev" + std::to_string(bus) + ".0";
    int fd = open(device.c_str(), O_RDWR);
    if (fd < 0) {
        setError("Failed to open SPI device: " + device);
        return false;
    }
    
    // Set mode
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        close(fd);
        setError("Failed to set SPI mode");
        return false;
    }
    
    // Set speed
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        close(fd);
        setError("Failed to set SPI speed");
        return false;
    }
    
    SPIInterface iface;
    iface.bus = bus;
    iface.speed = speed;
    iface.mode = mode;
    iface.fd = fd;
    spi_interfaces[bus] = iface;
    
    return true;
}

void GPIOController::spiSelect(int chipSelect) {
    current_spi_cs = chipSelect;
}

void GPIOController::spiDeselect(int chipSelect) {
    if (current_spi_cs == chipSelect) {
        current_spi_cs = -1;
    }
}

std::vector<uint8_t> GPIOController::spiTransfer(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    std::vector<uint8_t> result(data.size());
    
    if (spi_interfaces.empty()) {
        setError("No SPI interface initialized");
        return result;
    }
    
    // Use first available SPI interface
    auto& iface = spi_interfaces.begin()->second;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)data.data(),
        .rx_buf = (unsigned long)result.data(),
        .len = (uint32_t)data.size(),
        .speed_hz = (uint32_t)iface.speed,
        .delay_usecs = 0,
        .bits_per_word = 8,
        .cs_change = 0,
        .tx_nbits = 0,
        .rx_nbits = 0,
        .pad = 0
    };
    
    if (ioctl(iface.fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        setError("Failed to transfer SPI data");
        result.clear();
    }
    
    return result;
}

std::vector<uint8_t> GPIOController::spiRead(size_t length) {
    std::vector<uint8_t> data(length, 0);
    return spiTransfer(data);
}

bool GPIOController::spiWrite(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(gpio_mutex);
    
    if (spi_interfaces.empty()) {
        setError("No SPI interface initialized");
        return false;
    }
    
    auto& iface = spi_interfaces.begin()->second;
    
    if (write(iface.fd, data.data(), data.size()) != (ssize_t)data.size()) {
        setError("Failed to write SPI data");
        return false;
    }
    
    return true;
}

bool GPIOController::spiWriteRead(const std::vector<uint8_t>& writeData,
                                 std::vector<uint8_t>& readData) {
    readData = spiTransfer(writeData);
    return !readData.empty();
}

// ============================================
// REGISTER ACCESS
// ============================================

uint32_t GPIOController::readRegister(uint32_t offset) const {
    if (regs == nullptr) {
        return 0;
    }
    return *(volatile uint32_t*)((uintptr_t)regs + offset);
}

void GPIOController::writeRegister(uint32_t offset, uint32_t value) {
    if (regs == nullptr) {
        return;
    }
    *(volatile uint32_t*)((uintptr_t)regs + offset) = value;
}

void GPIOController::setRegisterBits(uint32_t offset, uint32_t mask) {
    if (regs == nullptr) {
        return;
    }
    volatile uint32_t* reg = (volatile uint32_t*)((uintptr_t)regs + offset);
    *reg |= mask;
}

void GPIOController::clearRegisterBits(uint32_t offset, uint32_t mask) {
    if (regs == nullptr) {
        return;
    }
    volatile uint32_t* reg = (volatile uint32_t*)((uintptr_t)regs + offset);
    *reg &= ~mask;
}

// ============================================
// PIN FUNCTIONS
// ============================================

void GPIOController::setPinFunction(int pin, int function) {
    int reg = pin / 10;
    int shift = (pin % 10) * 3;
    
    regs->GPFSEL[reg] &= ~(0b111 << shift);
    regs->GPFSEL[reg] |= (function << shift);
}

int GPIOController::getPinFunction(int pin) const {
    int reg = pin / 10;
    int shift = (pin % 10) * 3;
    return (regs->GPFSEL[reg] >> shift) & 0b111;
}

void GPIOController::setPullMode(int pin, int mode) {
    // Write the pull mode to the control register
    regs->GPPUD = mode;
    
    // Wait for control to settle
    delayMicroseconds(1);
    
    // Clock the pull mode into the pin
    int reg = pin / 32;
    int bit = pin % 32;
    regs->GPPUDCLK[reg] = 1 << bit;
    
    // Wait for clock to settle
    delayMicroseconds(1);
    
    // Clear the clock
    regs->GPPUD = 0;
    regs->GPPUDCLK[reg] = 0;
}

int GPIOController::getPullMode(int pin) const {
    // This is a simplified implementation
    // Actual hardware doesn't support reading pull mode directly
    // Return the stored value
    auto it = pin_configs.find(pin);
    if (it != pin_configs.end()) {
        return static_cast<int>(it->second.pull);
    }
    return 0;
}

// ============================================
// UTILITY
// ============================================

bool GPIOController::isPinValid(int pin) const {
    return pin >= 0 && pin < MAX_PINS;
}

bool GPIOController::isPinPWM(int pin) const {
    // Pins that support PWM on Raspberry Pi 5
    return (pin == 12 || pin == 13 || pin == 18 || pin == 19);
}

bool GPIOController::isPinSPI(int pin) const {
    // SPI pins
    return (pin == 7 || pin == 8 || pin == 9 || pin == 10 || pin == 11);
}

bool GPIOController::isPinI2C(int pin) const {
    // I2C pins
    return (pin == 2 || pin == 3);
}

void GPIOController::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

void GPIOController::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex);
    last_error = error;
    notifyError(error);
}

void GPIOController::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
}

void GPIOController::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void GPIOController::clearError() {
    std::lock_guard<std::mutex> lock(error_mutex);
    last_error.clear();
}

std::string GPIOController::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error;
}

uint64_t GPIOController::getCurrentTimeMicros() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
}

void GPIOController::updatePinInfo(int pin) {
    if (!isPinValid(pin)) {
        return;
    }
    
    auto it = pin_info.find(pin);
    if (it == pin_info.end()) {
        return;
    }
    
    PinInfo& info = it->second;
    
    // Update value
    info.value = readPin(pin);
    
    // Update direction
    int func = getPinFunction(pin);
    if (func <= 1) {
        info.direction = static_cast<PinDirection>(func);
    } else {
        info.direction = PinDirection::ALT0;
    }
    
    // Update pull mode
    info.pull = static_cast<PullMode>(getPullMode(pin));
    
    // Update interrupt status
    info.interrupt_enabled = isInterruptEnabled(pin);
    auto config_it = pin_configs.find(pin);
    if (config_it != pin_configs.end()) {
        info.interrupt_mode = config_it->second.interrupt_mode;
    }
}

void GPIOController::updateAllPinInfo() {
    for (int i = 0; i < MAX_PINS; i++) {
        updatePinInfo(i);
    }
}

std::string GPIOController::getPinStateString(int pin) const {
    bool value = readPin(pin);
    return value ? "HIGH" : "LOW";
}

void GPIOController::printPinState(int pin) const {
    if (!isPinValid(pin)) {
        std::cout << "Invalid pin: " << pin << std::endl;
        return;
    }
    
    auto info = getPinInfo(pin);
    std::cout << "Pin " << pin << " (" << info.name << "):" << std::endl;
    std::cout << "  Direction: " << getDirectionName(info.direction) << std::endl;
    std::cout << "  Pull: " << getPullModeName(info.pull) << std::endl;
    std::cout << "  Value: " << getPinStateString(pin) << std::endl;
    std::cout << "  Interrupt: " << (info.interrupt_enabled ? "Enabled" : "Disabled") << std::endl;
    if (info.interrupt_enabled) {
        std::cout << "  Interrupt Mode: " << getInterruptModeName(info.interrupt_mode) << std::endl;
    }
    std::cout << "  Changes: " << info.change_count << std::endl;
}

void GPIOController::printAllPinStates() const {
    std::cout << "\nGPIO Pin States:" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Pin  Name         Direction    Pull    Value  Interrupt" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;
    
    for (int i = 0; i < MAX_PINS; i++) {
        auto info = getPinInfo(i);
        std::string value = info.value ? "HIGH " : "LOW  ";
        std::string interrupt = info.interrupt_enabled ? "Yes  " : "No   ";
        
        printf("%3d  %-12s %-10s %-6s %-5s %s\n",
               i,
               info.name.c_str(),
               getDirectionName(info.direction).c_str(),
               getPullModeName(info.pull).c_str(),
               value.c_str(),
               interrupt.c_str());
    }
    std::cout << "============================================================" << std::endl;
}

bool GPIOController::exportPin(int pin) {
    std::string path = "/sys/class/gpio/export";
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << pin;
    return true;
}

bool GPIOController::unexportPin(int pin) {
    std::string path = "/sys/class/gpio/unexport";
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << pin;
    return true;
}

std::string GPIOController::getDirectionName(PinDirection direction) const {
    switch (direction) {
        case PinDirection::INPUT: return "INPUT";
        case PinDirection::OUTPUT: return "OUTPUT";
        case PinDirection::ALT0: return "ALT0";
        case PinDirection::ALT1: return "ALT1";
        case PinDirection::ALT2: return "ALT2";
        case PinDirection::ALT3: return "ALT3";
        case PinDirection::ALT4: return "ALT4";
        case PinDirection::ALT5: return "ALT5";
        default: return "UNKNOWN";
    }
}

std::string GPIOController::getPullModeName(PullMode pull) const {
    switch (pull) {
        case PullMode::NONE: return "NONE";
        case PullMode::PULL_UP: return "UP";
        case PullMode::PULL_DOWN: return "DOWN";
        default: return "UNKNOWN";
    }
}

std::string GPIOController::getInterruptModeName(InterruptMode mode) const {
    switch (mode) {
        case InterruptMode::EDGE_NONE: return "NONE";
        case InterruptMode::EDGE_RISING: return "RISING";
        case InterruptMode::EDGE_FALLING: return "FALLING";
        case InterruptMode::EDGE_BOTH: return "BOTH";
        case InterruptMode::LEVEL_HIGH: return "LEVEL_HIGH";
        case InterruptMode::LEVEL_LOW: return "LEVEL_LOW";
        default: return "UNKNOWN";
    }
}
