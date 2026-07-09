#include "SPIInterface.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/spi/spidev.h>
#include <linux/spi/spi.h>

// ============================================
// SINGLETON
// ============================================

SPIInterface& SPIInterface::getInstance() {
    static SPIInterface instance;
    return instance;
}

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

SPIInterface::SPIInterface() {
    // Initialize with default bus
    initialize(0);
}

SPIInterface::~SPIInterface() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool SPIInterface::initialize(int default_bus) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    // Open default bus
    SPIConfig config;
    config.bus = default_bus;
    config.chip_select = 0;
    if (openBus(config)) {
        initialized = true;
        std::cout << "SPI Interface initialized (bus " << default_bus << ")" << std::endl;
        return true;
    }
    
    std::cerr << "Failed to initialize SPI Interface" << std::endl;
    return false;
}

void SPIInterface::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Close all buses
    for (const auto& [bus, handle] : buses) {
        closeDevice(bus);
    }
    buses.clear();
    
    initialized = false;
    std::cout << "SPI Interface shutdown" << std::endl;
}

// ============================================
// BUS MANAGEMENT
// ============================================

bool SPIInterface::openBus(const SPIConfig& config) {
    std::lock_guard<std::mutex> lock(mutex);
    
    int bus = config.bus;
    int cs = config.chip_select;
    
    // Check if already open
    if (buses.find(bus) != buses.end()) {
        // Update configuration
        return configureDevice(bus, config);
    }
    
    BusHandle handle;
    handle.bus = bus;
    handle.fd = -1;
    handle.config = config;
    handle.state = BusState::CLOSED;
    handle.current_cs = -1;
    
    // Open device
    if (!openDevice(bus, cs)) {
        return false;
    }
    
    // Configure device
    if (!configureDevice(bus, config)) {
        closeDevice(bus);
        return false;
    }
    
    handle.state = BusState::OPEN;
    buses[bus] = handle;
    
    return true;
}

bool SPIInterface::openBus(int bus, int chip_select, int speed, Mode mode) {
    SPIConfig config;
    config.bus = bus;
    config.chip_select = chip_select;
    config.speed_hz = speed;
    config.mode = mode;
    return openBus(config);
}

void SPIInterface::closeBus(int bus) {
    std::lock_guard<std::mutex> lock(mutex);
    closeDevice(bus);
    buses.erase(bus);
}

bool SPIInterface::isBusOpen(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = buses.find(bus);
    return it != buses.end() && it->second.state == BusState::OPEN;
}

SPIInterface::BusInfo SPIInterface::getBusInfo(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    BusInfo info;
    info.bus = bus;
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        info.state = it->second.state;
        info.config = it->second.config;
        info.error_count = it->second.stats.error_count;
        info.bytes_transferred = it->second.stats.bytes_read + it->second.stats.bytes_written;
    } else {
        info.state = BusState::CLOSED;
    }
    
    return info;
}

std::vector<SPIInterface::BusInfo> SPIInterface::getAllBusInfo() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<BusInfo> result;
    for (const auto& [bus, handle] : buses) {
        BusInfo info;
        info.bus = bus;
        info.state = handle.state;
        info.config = handle.config;
        info.error_count = handle.stats.error_count;
        info.bytes_transferred = handle.stats.bytes_read + handle.stats.bytes_written;
        result.push_back(info);
    }
    return result;
}

bool SPIInterface::updateConfig(int bus, const SPIConfig& config) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return false;
    }
    
    if (!configureDevice(bus, config)) {
        return false;
    }
    
    it->second.config = config;
    return true;
}

// ============================================
// SPI OPERATIONS
// ============================================

std::vector<uint8_t> SPIInterface::transfer(int bus, const std::vector<uint8_t>& tx_data) {
    return transferImpl(bus, tx_data, nullptr);
}

std::vector<uint8_t> SPIInterface::transfer(int bus, const std::vector<uint8_t>& tx_data, const SPIConfig& config) {
    return transferImpl(bus, tx_data, &config);
}

bool SPIInterface::write(int bus, const std::vector<uint8_t>& data) {
    auto result = transfer(bus, data);
    return !result.empty();
}

std::vector<uint8_t> SPIInterface::read(int bus, size_t length) {
    std::vector<uint8_t> tx_data(length, 0);
    return transfer(bus, tx_data);
}

std::vector<uint8_t> SPIInterface::writeRead(int bus, const std::vector<uint8_t>& write_data, size_t read_length) {
    std::vector<uint8_t> tx_data = write_data;
    tx_data.resize(write_data.size() + read_length, 0);
    auto result = transfer(bus, tx_data);
    
    if (result.size() > write_data.size()) {
        return std::vector<uint8_t>(result.begin() + write_data.size(), result.end());
    }
    return std::vector<uint8_t>();
}

std::vector<std::vector<uint8_t>> SPIInterface::transferMultiple(
    int bus, const std::vector<std::vector<uint8_t>>& tx_buffers) {
    
    std::vector<std::vector<uint8_t>> results;
    
    for (const auto& tx_data : tx_buffers) {
        auto rx_data = transfer(bus, tx_data);
        results.push_back(rx_data);
    }
    
    return results;
}

// ============================================
// CHIP SELECT CONTROL
// ============================================

void SPIInterface::selectChip(int bus, int chip_select) {
    std::lock_guard<std::mutex> lock(mutex);
    setChipSelect(bus, chip_select, true);
}

void SPIInterface::deselectChip(int bus, int chip_select) {
    std::lock_guard<std::mutex> lock(mutex);
    setChipSelect(bus, chip_select, false);
}

bool SPIInterface::isChipSelected(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = buses.find(bus);
    return it != buses.end() && it->second.current_cs >= 0;
}

int SPIInterface::getCurrentChipSelect(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = buses.find(bus);
    if (it != buses.end()) {
        return it->second.current_cs;
    }
    return -1;
}

// ============================================
// PRIVATE METHODS
// ============================================

std::vector<uint8_t> SPIInterface::transferImpl(int bus, const std::vector<uint8_t>& tx_data, const SPIConfig* config) {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<uint8_t> rx_data;
    
    if (!isBusValid(bus)) {
        notifyError(bus, "Bus not open");
        return rx_data;
    }
    
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return rx_data;
    }
    
    // Use provided config or bus config
    SPIConfig effective_config = config ? *config : it->second.config;
    
    // Configure device if using custom config
    if (config) {
        configureDevice(bus, *config);
    }
    
    // Prepare transfer
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data.data(),
        .rx_buf = (unsigned long)nullptr,
        .len = (uint32_t)tx_data.size(),
        .speed_hz = (uint32_t)effective_config.speed_hz,
        .delay_usecs = (uint16_t)effective_config.delay_us,
        .bits_per_word = (uint8_t)effective_config.bits_per_word,
        .cs_change = 0,
        .tx_nbits = 0,
        .rx_nbits = 0,
        .pad = 0
    };
    
    rx_data.resize(tx_data.size());
    tr.rx_buf = (unsigned long)rx_data.data();
    
    SPITransfer transfer;
    transfer.tx_data = tx_data;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Perform transfer
    int ret = ioctl(it->second.fd, SPI_IOC_MESSAGE(1), &tr);
    
    auto end_time = std::chrono::steady_clock::now();
    transfer.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    
    if (ret < 0) {
        transfer.success = false;
        transfer.error_code = errno;
        notifyError(bus, "Transfer failed: " + std::string(strerror(errno)));
        recordTransfer(bus, transfer);
        return std::vector<uint8_t>();
    }
    
    transfer.success = true;
    transfer.rx_data = rx_data;
    recordTransfer(bus, transfer);
    
    return rx_data;
}

bool SPIInterface::openDevice(int bus, int chip_select) {
    std::string path = getDevicePath(bus, chip_select);
    
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open SPI device: " << path << " (" << strerror(errno) << ")" << std::endl;
        return false;
    }
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        close(it->second.fd);
        it->second.fd = fd;
        it->second.state = BusState::OPEN;
        it->second.current_cs = chip_select;
    } else {
        BusHandle handle;
        handle.bus = bus;
        handle.fd = fd;
        handle.config.bus = bus;
        handle.config.chip_select = chip_select;
        handle.state = BusState::OPEN;
        handle.current_cs = chip_select;
        buses[bus] = handle;
    }
    
    return true;
}

void SPIInterface::closeDevice(int bus) {
    auto it = buses.find(bus);
    if (it != buses.end()) {
        if (it->second.fd >= 0) {
            close(it->second.fd);
            it->second.fd = -1;
        }
        it->second.state = BusState::CLOSED;
        it->second.current_cs = -1;
    }
}

bool SPIInterface::configureDevice(int bus, const SPIConfig& config) {
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return false;
    }
    
    int fd = it->second.fd;
    
    // Set mode
    uint8_t mode = static_cast<uint8_t>(config.mode);
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        std::cerr << "Failed to set SPI mode: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set bits per word
    uint8_t bits = static_cast<uint8_t>(config.bits_per_word);
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        std::cerr << "Failed to set bits per word: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set speed
    uint32_t speed = static_cast<uint32_t>(config.speed_hz);
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        std::cerr << "Failed to set SPI speed: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set LSB first
    uint8_t lsb = config.lsb_first ? 1 : 0;
    if (ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb) < 0) {
        std::cerr << "Failed to set LSB first: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool SPIInterface::setChipSelect(int bus, int chip_select, bool select) {
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return false;
    }
    
    // For hardware CS, this is handled automatically by the SPI driver
    // For software CS, we would toggle GPIO pins here
    if (select) {
        it->second.current_cs = chip_select;
    } else {
        it->second.current_cs = -1;
    }
    
    return true;
}

void SPIInterface::recordTransfer(int bus, const SPITransfer& transfer) {
    auto it = buses.find(bus);
    if (it == buses.end()) {
        return;
    }
    
    it->second.stats.total_transfers++;
    if (transfer.success) {
        it->second.stats.successful_transfers++;
        it->second.stats.bytes_written += transfer.tx_data.size();
        it->second.stats.bytes_read += transfer.rx_data.size();
    } else {
        it->second.stats.failed_transfers++;
        it->second.stats.error_count++;
    }
    
    if (on_transfer) {
        on_transfer(transfer);
    }
}

void SPIInterface::notifyError(int bus, const std::string& error) {
    if (on_error) {
        on_error(bus, error);
    }
}

// ============================================
// STATISTICS
// ============================================

SPIInterface::Stats SPIInterface::getStats(int bus) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        return it->second.stats;
    }
    return Stats{};
}

SPIInterface::Stats SPIInterface::getTotalStats() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    Stats total;
    for (const auto& [bus, handle] : buses) {
        total.total_transfers += handle.stats.total_transfers;
        total.successful_transfers += handle.stats.successful_transfers;
        total.failed_transfers += handle.stats.failed_transfers;
        total.bytes_written += handle.stats.bytes_written;
        total.bytes_read += handle.stats.bytes_read;
        total.error_count += handle.stats.error_count;
    }
    return total;
}

void SPIInterface::resetStats(int bus) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = buses.find(bus);
    if (it != buses.end()) {
        it->second.stats = Stats{};
    }
}

void SPIInterface::resetAllStats() {
    std::lock_guard<std::mutex> lock(mutex);
    
    for (auto& [bus, handle] : buses) {
        handle.stats = Stats{};
    }
}

// ============================================
// CALLBACKS
// ============================================

void SPIInterface::setOnTransfer(TransferCallback callback) {
    on_transfer = callback;
}

void SPIInterface::setOnError(ErrorCallback callback) {
    on_error = callback;
}

// ============================================
// UTILITY
// ============================================

std::string SPIInterface::getDevicePath(int bus, int chip_select) const {
    return "/dev/spidev" + std::to_string(bus) + "." + std::to_string(chip_select);
}

int SPIInterface::getSPIFd(int bus) const {
    auto it = buses.find(bus);
    if (it != buses.end()) {
        return it->second.fd;
    }
    return -1;
}

bool SPIInterface::isBusValid(int bus) const {
    auto it = buses.find(bus);
    return it != buses.end() && it->second.state == BusState::OPEN && it->second.fd >= 0;
}

void SPIInterface::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

std::string SPIInterface::getModeName(Mode mode) const {
    switch (mode) {
        case Mode::MODE_0: return "Mode 0 (CPOL=0, CPHA=0)";
        case Mode::MODE_1: return "Mode 1 (CPOL=0, CPHA=1)";
        case Mode::MODE_2: return "Mode 2 (CPOL=1, CPHA=0)";
        case Mode::MODE_3: return "Mode 3 (CPOL=1, CPHA=1)";
        default: return "Unknown";
    }
}

std::string SPIInterface::getStateName(BusState state) const {
    switch (state) {
        case BusState::CLOSED: return "Closed";
        case BusState::OPEN: return "Open";
        case BusState::ERROR: return "Error";
        default: return "Unknown";
    }
}

bool SPIInterface::testDevice(int bus, const std::vector<uint8_t>& test_data) {
    std::cout << "Testing SPI device on bus " << bus << std::endl;
    
    if (!isBusOpen(bus)) {
        std::cout << "  Bus not open" << std::endl;
        return false;
    }
    
    std::vector<uint8_t> data = test_data.empty() ? std::vector<uint8_t>{0x55, 0xAA, 0xFF, 0x00} : test_data;
    
    std::cout << "  Sending: ";
    for (uint8_t b : data) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    std::cout << std::endl;
    
    auto result = transfer(bus, data);
    
    std::cout << "  Received: ";
    for (uint8_t b : result) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    std::cout << std::endl;
    
    return !result.empty();
}

int SPIInterface::getMaxSpeed(int bus) const {
    // The maximum speed depends on the device and bus
    // For Raspberry Pi 5, the SPI bus can go up to 125 MHz
    return 125000000;
}
