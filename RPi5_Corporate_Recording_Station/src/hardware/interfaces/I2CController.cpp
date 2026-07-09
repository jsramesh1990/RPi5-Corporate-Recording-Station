#include "I2CController.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

// Device identification database
const std::vector<I2CController::DeviceDB> I2CController::device_database = {
    {0x18, "LM75", "Temperature Sensor", 0x01, 0x01},
    {0x19, "TMP102", "Temperature Sensor", 0x01, 0x02},
    {0x1A, "Camera", "Raspberry Pi Camera Module", 0x02, 0x01},
    {0x1E, "LSM303", "Accelerometer/Magnetometer", 0x03, 0x01},
    {0x28, "BMP280", "Barometric Pressure Sensor", 0x04, 0x01},
    {0x29, "HMC5883L", "Magnetometer", 0x05, 0x01},
    {0x40, "SHT31", "Humidity/Temperature Sensor", 0x06, 0x01},
    {0x48, "ADS1115", "16-bit ADC", 0x07, 0x01},
    {0x50, "EEPROM", "24C32 EEPROM", 0x08, 0x01},
    {0x68, "DS3231", "Real Time Clock", 0x09, 0x01},
    {0x77, "BME280", "Temperature/Humidity/Pressure", 0x04, 0x02},
    {0x20, "MCP23017", "16-bit GPIO Expander", 0x0A, 0x01},
    {0x21, "MCP23017", "16-bit GPIO Expander", 0x0A, 0x02},
    {0x22, "PCA9685", "PWM Driver", 0x0B, 0x01},
    {0x60, "SSD1306", "OLED Display", 0x0C, 0x01},
    {0x70, "HT16K33", "LED Driver", 0x0D, 0x01}
};

// ============================================
// SINGLETON
// ============================================

I2CController& I2CController::getInstance() {
    static I2CController instance;
    return instance;
}

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

I2CController::I2CController() {
    // Initialize with default bus
    initialize(1);
}

I2CController::~I2CController() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool I2CController::initialize(int default_bus) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    // Open default bus
    if (openBus(default_bus, Speed::STANDARD)) {
        initialized = true;
        std::cout << "I2C Controller initialized (bus " << default_bus << ")" << std::endl;
        return true;
    }
    
    std::cerr << "Failed to initialize I2C Controller" << std::endl;
    return false;
}

void I2CController::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Close all buses
    for (auto& [bus, handle] : buses) {
        closeDevice(bus);
    }
    buses.clear();
    
    initialized = false;
    std::cout << "I2C Controller shutdown" << std::endl;
}

// ============================================
// BUS MANAGEMENT
// ============================================

bool I2CController::openBus(int bus, Speed speed) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (buses.find(bus) != buses.end()) {
        // Bus already open, update speed if needed
        if (buses[bus].speed != speed) {
            return setBusSpeed(bus, speed);
        }
        return true;
    }
    
    BusHandle handle;
    handle.bus = bus;
    handle.fd = -1;
    handle.speed = speed;
    handle.state = BusState::CLOSED;
    
    // Open device
    if (!openDevice(bus)) {
        return false;
    }
    
    handle.state = BusState::OPEN;
    buses[bus] = handle;
    
    // Scan for devices
    scanBus(bus);
    
    return true;
}

void I2CController::closeBus(int bus) {
    std::lock_guard<std::mutex> lock(mutex);
    closeDevice(bus);
    buses.erase(bus);
}

bool I2CController::isBusOpen(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = buses.find(bus);
    return it != buses.end() && it->second.state == BusState::OPEN;
}

I2CController::BusInfo I2CController::getBusInfo(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    BusInfo info;
    info.bus_number = bus;
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        info.state = it->second.state;
        info.speed = it->second.speed;
        info.devices = it->second.devices;
        info.error_count = it->second.stats.error_count;
        info.bytes_transferred = it->second.stats.bytes_read + it->second.stats.bytes_written;
    } else {
        info.state = BusState::CLOSED;
    }
    
    return info;
}

std::vector<I2CController::BusInfo> I2CController::getAllBusInfo() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<BusInfo> result;
    for (const auto& [bus, handle] : buses) {
        BusInfo info;
        info.bus_number = bus;
        info.state = handle.state;
        info.speed = handle.speed;
        info.devices = handle.devices;
        info.error_count = handle.stats.error_count;
        info.bytes_transferred = handle.stats.bytes_read + handle.stats.bytes_written;
        result.push_back(info);
    }
    return result;
}

bool I2CController::setBusSpeed(int bus, Speed speed) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return false;
    }
    
    // Update speed
    if (ioctl(it->second.fd, I2C_SPEED, static_cast<int>(speed)) < 0) {
        std::cerr << "Failed to set I2C speed: " << strerror(errno) << std::endl;
        return false;
    }
    
    it->second.speed = speed;
    return true;
}

I2CController::Speed I2CController::getBusSpeed(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = buses.find(bus);
    if (it != buses.end()) {
        return it->second.speed;
    }
    return Speed::STANDARD;
}

// ============================================
// DEVICE DETECTION
// ============================================

std::vector<uint8_t> I2CController::scanBus(int bus) {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<uint8_t> devices;
    
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return devices;
    }
    
    // Clear existing devices
    it->second.devices.clear();
    
    for (int addr = 0x03; addr < 0x78; addr++) {
        if (setSlaveAddress(bus, addr)) {
            // Try to read 1 byte
            uint8_t test;
            if (read(it->second.fd, &test, 1) == 1) {
                devices.push_back(addr);
                updateDeviceInfo(bus, addr);
                notifyDeviceDetected(bus, addr);
            }
        }
    }
    
    return devices;
}

std::map<int, std::vector<uint8_t>> I2CController::scanAllBuses() {
    std::map<int, std::vector<uint8_t>> results;
    
    for (const auto& [bus, handle] : buses) {
        results[bus] = scanBus(bus);
    }
    
    return results;
}

bool I2CController::detectDevice(int bus, uint8_t address) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isAddressValid(address)) {
        return false;
    }
    
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return false;
    }
    
    if (!setSlaveAddress(bus, address)) {
        return false;
    }
    
    uint8_t test;
    if (read(it->second.fd, &test, 1) == 1) {
        updateDeviceInfo(bus, address);
        notifyDeviceDetected(bus, address);
        return true;
    }
    
    return false;
}

I2CController::DeviceInfo I2CController::getDeviceInfo(int bus, uint8_t address) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    DeviceInfo info;
    info.address = address;
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        for (const auto& dev : it->second.devices) {
            if (dev.address == address) {
                return dev;
            }
        }
    }
    
    // If not found, return basic info
    info.name = identifyDevice(address);
    info.detected = false;
    return info;
}

std::string I2CController::identifyDevice(uint8_t address) const {
    std::string name = identifyDeviceFromDB(address);
    if (!name.empty()) {
        return name;
    }
    
    // Try common device patterns
    if (address >= 0x50 && address <= 0x57) {
        return "EEPROM";
    } else if (address >= 0x40 && address <= 0x47) {
        return "ADC/DAC";
    } else if (address >= 0x60 && address <= 0x67) {
        return "Display/IO Expander";
    } else if (address >= 0x30 && address <= 0x37) {
        return "Sensor";
    }
    
    return "Unknown";
}

// ============================================
// I2C OPERATIONS
// ============================================

bool I2CController::write(int bus, uint8_t address, const std::vector<uint8_t>& data, int retry_count) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isBusValid(bus)) {
        notifyError(bus, "Bus not open");
        return false;
    }
    
    if (!isAddressValid(address)) {
        notifyError(bus, "Invalid address: 0x" + std::to_string(address));
        return false;
    }
    
    Transaction transaction;
    transaction.address = address;
    transaction.write_data = data;
    
    int attempts = 0;
    bool success = false;
    auto start_time = std::chrono::steady_clock::now();
    
    while (!success && (retry_count == 0 || attempts <= retry_count)) {
        attempts++;
        
        if (!setSlaveAddress(bus, address)) {
            continue;
        }
        
        auto it = buses.find(bus);
        if (it == buses.end()) {
            continue;
        }
        
        if (write(it->second.fd, data.data(), data.size()) == (ssize_t)data.size()) {
            success = true;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    transaction.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    transaction.success = success;
    transaction.error_code = errno;
    
    recordTransaction(bus, transaction);
    
    if (!success) {
        notifyError(bus, "Write failed after " + std::to_string(attempts) + " attempts");
    }
    
    return success;
}

bool I2CController::writeByte(int bus, uint8_t address, uint8_t value) {
    std::vector<uint8_t> data = {value};
    return write(bus, address, data);
}

bool I2CController::writeRegister(int bus, uint8_t address, uint8_t reg, uint8_t value) {
    std::vector<uint8_t> data = {reg, value};
    return write(bus, address, data);
}

bool I2CController::writeRegister16(int bus, uint8_t address, uint8_t reg, uint16_t value) {
    std::vector<uint8_t> data = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    return write(bus, address, data);
}

std::vector<uint8_t> I2CController::read(int bus, uint8_t address, size_t length, int retry_count) {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<uint8_t> result;
    
    if (!isBusValid(bus)) {
        notifyError(bus, "Bus not open");
        return result;
    }
    
    if (!isAddressValid(address)) {
        notifyError(bus, "Invalid address: 0x" + std::to_string(address));
        return result;
    }
    
    Transaction transaction;
    transaction.address = address;
    
    int attempts = 0;
    bool success = false;
    auto start_time = std::chrono::steady_clock::now();
    
    while (!success && (retry_count == 0 || attempts <= retry_count)) {
        attempts++;
        
        if (!setSlaveAddress(bus, address)) {
            continue;
        }
        
        auto it = buses.find(bus);
        if (it == buses.end()) {
            continue;
        }
        
        result.resize(length);
        if (read(it->second.fd, result.data(), length) == (ssize_t)length) {
            success = true;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    transaction.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    transaction.read_data = result;
    transaction.success = success;
    transaction.error_code = errno;
    
    recordTransaction(bus, transaction);
    
    if (!success) {
        notifyError(bus, "Read failed after " + std::to_string(attempts) + " attempts");
        result.clear();
    }
    
    return result;
}

uint8_t I2CController::readByte(int bus, uint8_t address) {
    auto data = read(bus, address, 1);
    return data.empty() ? 0 : data[0];
}

uint8_t I2CController::readRegister(int bus, uint8_t address, uint8_t reg) {
    writeByte(bus, address, reg);
    return readByte(bus, address);
}

uint16_t I2CController::readRegister16(int bus, uint8_t address, uint8_t reg) {
    writeByte(bus, address, reg);
    auto data = read(bus, address, 2);
    if (data.size() < 2) return 0;
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

std::vector<uint8_t> I2CController::readRegisters(int bus, uint8_t address, uint8_t start_reg, size_t count) {
    std::vector<uint8_t> result;
    
    if (!writeByte(bus, address, start_reg)) {
        return result;
    }
    
    return read(bus, address, count);
}

std::vector<uint8_t> I2CController::writeRead(int bus, uint8_t address,
                                              const std::vector<uint8_t>& write_data,
                                              size_t read_length) {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<uint8_t> result;
    
    if (!isBusValid(bus)) {
        return result;
    }
    
    if (!isAddressValid(address)) {
        return result;
    }
    
    if (!setSlaveAddress(bus, address)) {
        return result;
    }
    
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return result;
    }
    
    // Write
    if (!write_data.empty()) {
        if (write(it->second.fd, write_data.data(), write_data.size()) != (ssize_t)write_data.size()) {
            return result;
        }
    }
    
    // Read
    result.resize(read_length);
    if (read(it->second.fd, result.data(), read_length) != (ssize_t)read_length) {
        result.clear();
    }
    
    return result;
}

// ============================================
// BULK OPERATIONS
// ============================================

std::vector<I2CController::Transaction> I2CController::bulkTransactions(
    int bus, const std::vector<Transaction>& transactions) {
    
    std::vector<Transaction> results;
    
    for (const auto& trans : transactions) {
        Transaction result = trans;
        
        if (!trans.write_data.empty()) {
            result.success = write(bus, trans.address, trans.write_data);
        }
        
        if (!trans.read_data.empty()) {
            auto data = read(bus, trans.address, trans.read_data.size());
            result.read_data = data;
            result.success = !data.empty();
        }
        
        results.push_back(result);
    }
    
    return results;
}

size_t I2CController::broadcast(int bus, const std::vector<uint8_t>& addresses, const std::vector<uint8_t>& data) {
    size_t success_count = 0;
    
    for (uint8_t address : addresses) {
        if (write(bus, address, data)) {
            success_count++;
        }
    }
    
    return success_count;
}

// ============================================
// CALLBACKS
// ============================================

void I2CController::setOnDeviceDetected(DeviceDetectedCallback callback) {
    on_device_detected = callback;
}

void I2CController::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void I2CController::setOnTransaction(TransactionCallback callback) {
    on_transaction = callback;
}

// ============================================
// STATISTICS
// ============================================

I2CController::Stats I2CController::getStats(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        return it->second.stats;
    }
    return Stats{};
}

I2CController::Stats I2CController::getTotalStats() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    Stats total;
    for (const auto& [bus, handle] : buses) {
        total.total_transactions += handle.stats.total_transactions;
        total.successful_transactions += handle.stats.successful_transactions;
        total.failed_transactions += handle.stats.failed_transactions;
        total.bytes_written += handle.stats.bytes_written;
        total.bytes_read += handle.stats.bytes_read;
        total.error_count += handle.stats.error_count;
    }
    return total;
}

void I2CController::resetStats(int bus) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        it->second.stats = Stats{};
    }
}

void I2CController::resetAllStats() {
    std::lock_guard<std::mutex> lock(mutex);
    
    for (auto& [bus, handle] : buses) {
        handle.stats = Stats{};
    }
}

// ============================================
// PRIVATE METHODS
// ============================================

bool I2CController::openDevice(int bus) {
    std::string path = getDevicePath(bus);
    
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open I2C device: " << path << " (" << strerror(errno) << ")" << std::endl;
        return false;
    }
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        close(it->second.fd);
        it->second.fd = fd;
        it->second.state = BusState::OPEN;
    } else {
        BusHandle handle;
        handle.bus = bus;
        handle.fd = fd;
        handle.speed = Speed::STANDARD;
        handle.state = BusState::OPEN;
        buses[bus] = handle;
    }
    
    return true;
}

void I2CController::closeDevice(int bus) {
    auto it = buses.find(bus);
    if (it != buses.end()) {
        if (it->second.fd >= 0) {
            close(it->second.fd);
            it->second.fd = -1;
        }
        it->second.state = BusState::CLOSED;
    }
}

bool I2CController::setSlaveAddress(int bus, uint8_t address) {
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return false;
    }
    
    if (ioctl(it->second.fd, I2C_SLAVE, address) < 0) {
        return false;
    }
    
    return true;
}

void I2CController::updateDeviceInfo(int bus, uint8_t address) {
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return;
    }
    
    DeviceInfo info;
    info.address = address;
    info.detected = true;
    info.name = identifyDevice(address);
    
    // Check if already exists
    for (auto& dev : it->second.devices) {
        if (dev.address == address) {
            dev = info;
            return;
        }
    }
    
    it->second.devices.push_back(info);
}

void I2CController::recordTransaction(int bus, const Transaction& transaction) {
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return;
    }
    
    it->second.stats.total_transactions++;
    if (transaction.success) {
        it->second.stats.successful_transactions++;
        it->second.stats.bytes_written += transaction.write_data.size();
        it->second.stats.bytes_read += transaction.read_data.size();
    } else {
        it->second.stats.failed_transactions++;
        it->second.stats.error_count++;
    }
    
    if (on_transaction) {
        on_transaction(transaction);
    }
}

void I2CController::notifyError(int bus, const std::string& error) {
    if (on_error) {
        on_error(bus, error);
    }
}

void I2CController::notifyDeviceDetected(int bus, uint8_t address) {
    if (on_device_detected) {
        on_device_detected(bus, address);
    }
}

std::string I2CController::identifyDeviceFromDB(uint8_t address) const {
    for (const auto& dev : device_database) {
        if (dev.address == address) {
            return dev.name;
        }
    }
    return "";
}

std::string I2CController::getDevicePath(int bus) const {
    return "/dev/i2c-" + std::to_string(bus);
}

int I2CController::getI2CFd(int bus) const {
    auto it = buses.find(bus);
    if (it != buses.end()) {
        return it->second.fd;
    }
    return -1;
}

bool I2CController::isBusValid(int bus) const {
    auto it = buses.find(bus);
    return it != buses.end() && it->second.state == BusState::OPEN && it->second.fd >= 0;
}

bool I2CController::isAddressValid(uint8_t address) const {
    return address >= 0x03 && address < 0x78;
}

void I2CController::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

// ============================================
// UTILITY
// ============================================

bool I2CController::deviceExists(int bus, uint8_t address) {
    return detectDevice(bus, address);
}

std::string I2CController::getDeviceName(uint8_t address) const {
    return identifyDevice(address);
}

std::string I2CController::addressToString(uint8_t address) const {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(address);
    return ss.str();
}

std::string I2CController::getSpeedName(Speed speed) const {
    switch (speed) {
        case Speed::STANDARD: return "Standard (100 kHz)";
        case Speed::FAST: return "Fast (400 kHz)";
        case Speed::FAST_PLUS: return "Fast+ (1 MHz)";
        case Speed::HIGH_SPEED: return "High Speed (3.4 MHz)";
        default: return "Unknown";
    }
}

std::string I2CController::getStateName(BusState state) const {
    switch (state) {
        case BusState::CLOSED: return "Closed";
        case BusState::OPEN: return "Open";
        case BusState::ERROR: return "Error";
        default: return "Unknown";
    }
}

std::string I2CController::dumpRegisters(int bus, uint8_t address, uint8_t start_reg, size_t count) {
    std::stringstream ss;
    
    ss << "Register dump for device 0x" << std::hex << std::setw(2) << std::setfill('0') 
       << static_cast<int>(address) << " (bus " << bus << "):\n";
    ss << "------------------------------------------------\n";
    ss << "Reg  | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n";
    ss << "-----+------------------------------------------------\n";
    
    for (size_t offset = 0; offset < count; offset += 16) {
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
           << (start_reg + offset) << " | ";
        
        for (size_t i = 0; i < 16 && (offset + i) < count; i++) {
            uint8_t value = readRegister(bus, address, start_reg + offset + i);
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value) << " ";
        }
        ss << "\n";
    }
    
    return ss.str();
}

bool I2CController::testDevice(int bus, uint8_t address) {
    std::cout << "Testing I2C device 0x" << std::hex << static_cast<int>(address) 
              << " on bus " << bus << std::endl;
    
    // Check if device exists
    if (!detectDevice(bus, address)) {
        std::cout << "  Device not detected" << std::endl;
        return false;
    }
    
    std::cout << "  Device detected: " << getDeviceName(address) << std::endl;
    
    // Try to read device ID (if available)
    try {
        // Try common ID register addresses
        for (uint8_t id_reg : {0x00, 0x01, 0x02, 0x03}) {
            uint8_t value = readRegister(bus, address, id_reg);
            if (value != 0) {
                std::cout << "  Register 0x" << std::hex << id_reg << " = 0x" << value << std::endl;
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return true;
}
