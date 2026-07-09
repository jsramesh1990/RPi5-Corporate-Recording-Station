#include "PL011UART.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <cstdarg>

PL011UART::PL011UART() 
    : state(UARTState::CLOSED), uart_fd(-1), 
      baremetal_mode(false), uart_memory(nullptr), uart_base(0),
      async_read_running(false), async_write_running(false) {
}

PL011UART::~PL011UART() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool PL011UART::initialize(const UARTConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state != UARTState::CLOSED) {
        shutdown();
    }
    
    config = cfg;
    
    // Check if device exists
    if (!isDeviceAvailable()) {
        notifyError("Device not available: " + config.device);
        return false;
    }
    
    // Open device
    if (!openDevice()) {
        return false;
    }
    
    // Configure device
    if (!configureDevice()) {
        closeDevice();
        return false;
    }
    
    state = UARTState::CONFIGURED;
    resetStats();
    
    std::cout << "PL011 UART initialized: " << config.device 
              << " @ " << config.baud_rate << " baud" << std::endl;
    
    return true;
}

bool PL011UART::initialize(const std::string& device, int baud_rate,
                          DataBits data_bits, StopBits stop_bits, Parity parity) {
    UARTConfig cfg;
    cfg.device = device;
    cfg.baud_rate = baud_rate;
    cfg.data_bits = data_bits;
    cfg.stop_bits = stop_bits;
    cfg.parity = parity;
    return initialize(cfg);
}

bool PL011UART::initializeDirect(uint32_t base_addr, int baud_rate) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state != UARTState::CLOSED) {
        shutdown();
    }
    
    uart_base = base_addr;
    baremetal_mode = true;
    
    // Setup bare-metal UART
    if (!setupBaremetal()) {
        notifyError("Failed to setup bare-metal UART");
        return false;
    }
    
    if (!configureBaremetal(baud_rate)) {
        shutdownBaremetal();
        return false;
    }
    
    config.baud_rate = baud_rate;
    config.device = "Baremetal";
    state = UARTState::CONFIGURED;
    resetStats();
    
    std::cout << "PL011 UART initialized (bare-metal) @ " << baud_rate << " baud" << std::endl;
    return true;
}

void PL011UART::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Stop async operations
    if (async_read_running) {
        stopAsyncRead();
    }
    if (async_write_running) {
        stopAsyncWrite();
    }
    
    if (baremetal_mode) {
        shutdownBaremetal();
    } else {
        closeDevice();
    }
    
    state = UARTState::CLOSED;
    std::cout << "PL011 UART shutdown" << std::endl;
}

// ============================================
// DEVICE I/O
// ============================================

bool PL011UART::openDevice() {
    uart_fd = open(config.device.c_str(), O_RDWR | O_NOCTTY);
    if (uart_fd < 0) {
        notifyError("Failed to open device: " + config.device + " (" + strerror(errno) + ")");
        return false;
    }
    return true;
}

void PL011UART::closeDevice() {
    if (uart_fd >= 0) {
        close(uart_fd);
        uart_fd = -1;
    }
}

bool PL011UART::configureDevice() {
    struct termios options;
    
    if (tcgetattr(uart_fd, &options) < 0) {
        notifyError("Failed to get terminal attributes");
        return false;
    }
    
    // Set baud rate
    speed_t speed = static_cast<speed_t>(getBaudFlag(config.baud_rate));
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    
    // Set data bits
    options.c_cflag &= ~CSIZE;
    switch (config.data_bits) {
        case DataBits::FIVE: options.c_cflag |= CS5; break;
        case DataBits::SIX: options.c_cflag |= CS6; break;
        case DataBits::SEVEN: options.c_cflag |= CS7; break;
        case DataBits::EIGHT: options.c_cflag |= CS8; break;
    }
    
    // Set parity
    switch (config.parity) {
        case Parity::NONE:
            options.c_cflag &= ~PARENB;
            break;
        case Parity::EVEN:
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            break;
        case Parity::ODD:
            options.c_cflag |= PARENB;
            options.c_cflag |= PARODD;
            break;
        default:
            options.c_cflag &= ~PARENB;
            break;
    }
    
    // Set stop bits
    if (config.stop_bits == StopBits::TWO) {
        options.c_cflag |= CSTOPB;
    } else {
        options.c_cflag &= ~CSTOPB;
    }
    
    // Set flow control
    if (config.flow_control == FlowControl::RTS_CTS) {
        options.c_cflag |= CRTSCTS;
    } else {
        options.c_cflag &= ~CRTSCTS;
    }
    
    // Set local and canonical modes
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    options.c_oflag &= ~OPOST;
    
    // Set read timeout
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = config.read_timeout_ms / 100;
    
    // Apply settings
    if (tcsetattr(uart_fd, TCSANOW, &options) < 0) {
        notifyError("Failed to set terminal attributes");
        return false;
    }
    
    // Flush buffers
    tcflush(uart_fd, TCIOFLUSH);
    
    return true;
}

// ============================================
// DATA TRANSMISSION
// ============================================

bool PL011UART::writeData(const std::vector<uint8_t>& data) {
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    if (baremetal_mode) {
        writeBaremetal(data.data(), data.size());
        updateStatsSent(data.size());
        return true;
    }
    
    if (uart_fd < 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex);
    
    ssize_t written = write(uart_fd, data.data(), data.size());
    if (written < 0) {
        stats.send_errors++;
        notifyError("Write error: " + std::string(strerror(errno)));
        return false;
    }
    
    updateStatsSent(written);
    return true;
}

bool PL011UART::writeData(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return writeData(bytes);
}

bool PL011UART::writeByte(uint8_t byte) {
    std::vector<uint8_t> data = {byte};
    return writeData(data);
}

bool PL011UART::writeLine(const std::string& line) {
    if (!writeData(line)) {
        return false;
    }
    return writeData("\r\n");
}

bool PL011UART::writeFormatted(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return writeData(std::string(buffer));
}

bool PL011UART::writeHexDump(const std::vector<uint8_t>& data, const std::string& prefix) {
    std::stringstream ss;
    int bytes_per_line = 16;
    
    for (size_t i = 0; i < data.size(); i += bytes_per_line) {
        // Address
        ss << prefix << std::hex << std::setw(8) << std::setfill('0') << i << ": ";
        
        // Hex
        for (size_t j = 0; j < bytes_per_line && i + j < data.size(); j++) {
            ss << std::hex << std::setw(2) << std::setfill('0') 
               << static_cast<int>(data[i + j]) << " ";
        }
        
        // Padding
        size_t remaining = bytes_per_line - (data.size() - i);
        for (size_t j = 0; j < remaining && remaining < bytes_per_line; j++) {
            ss << "   ";
        }
        
        // ASCII
        ss << " | ";
        for (size_t j = 0; j < bytes_per_line && i + j < data.size(); j++) {
            uint8_t c = data[i + j];
            ss << (isprint(c) ? static_cast<char>(c) : '.');
        }
        
        ss << "\n";
    }
    
    return writeData(ss.str());
}

// ============================================
// DATA RECEPTION
// ============================================

std::vector<uint8_t> PL011UART::readData(size_t length) {
    std::vector<uint8_t> data;
    
    if (state == UARTState::CLOSED) {
        return data;
    }
    
    if (baremetal_mode) {
        data.resize(length);
        size_t read = readBaremetal(data.data(), length);
        data.resize(read);
        updateStatsReceived(read);
        return data;
    }
    
    if (uart_fd < 0) {
        return data;
    }
    
    std::lock_guard<std::mutex> lock(mutex);
    
    data.resize(length);
    ssize_t read = ::read(uart_fd, data.data(), length);
    if (read < 0) {
        if (errno != EAGAIN) {
            stats.receive_errors++;
            notifyError("Read error: " + std::string(strerror(errno)));
        }
        return std::vector<uint8_t>();
    }
    
    data.resize(read);
    updateStatsReceived(read);
    return data;
}

std::string PL011UART::readLine() {
    std::string line;
    const int max_line_length = 4096;
    
    while (line.length() < max_line_length) {
        uint8_t c = readByte();
        if (c == '\n' || c == '\r') {
            if (!line.empty()) {
                break;
            }
            continue;
        }
        line += static_cast<char>(c);
    }
    
    return line;
}

std::string PL011UART::readAll() {
    std::string result;
    
    while (dataAvailable()) {
        auto data = readData(1024);
        result.append(data.begin(), data.end());
    }
    
    return result;
}

uint8_t PL011UART::readByte() {
    auto data = readData(1);
    return data.empty() ? 0 : data[0];
}

bool PL011UART::dataAvailable() const {
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    if (baremetal_mode) {
        return dataAvailableBaremetal();
    }
    
    if (uart_fd < 0) {
        return false;
    }
    
    int bytes_avail;
    if (ioctl(uart_fd, FIONREAD, &bytes_avail) < 0) {
        return false;
    }
    
    return bytes_avail > 0;
}

size_t PL011UART::bytesAvailable() const {
    if (state == UARTState::CLOSED) {
        return 0;
    }
    
    if (baremetal_mode) {
        return dataAvailableBaremetal() ? 1 : 0;
    }
    
    if (uart_fd < 0) {
        return 0;
    }
    
    int bytes_avail;
    if (ioctl(uart_fd, FIONREAD, &bytes_avail) < 0) {
        return 0;
    }
    
    return bytes_avail;
}

void PL011UART::flushInput() {
    if (uart_fd >= 0) {
        tcflush(uart_fd, TCIFLUSH);
    }
    if (baremetal_mode) {
        flushBaremetal();
    }
}

void PL011UART::flushOutput() {
    if (uart_fd >= 0) {
        tcflush(uart_fd, TCOFLUSH);
    }
}

void PL011UART::flushBoth() {
    if (uart_fd >= 0) {
        tcflush(uart_fd, TCIOFLUSH);
    }
    if (baremetal_mode) {
        flushBaremetal();
    }
}

// ============================================
// ASYNCHRONOUS OPERATIONS
// ============================================

bool PL011UART::startAsyncRead(DataCallback callback) {
    if (async_read_running) {
        return false;
    }
    
    data_callback = callback;
    async_read_running = true;
    async_read_thread = std::thread(&PL011UART::asyncReadLoop, this);
    
    return true;
}

void PL011UART::stopAsyncRead() {
    if (!async_read_running) {
        return;
    }
    
    async_read_running = false;
    if (async_read_thread.joinable()) {
        async_read_thread.join();
    }
}

bool PL011UART::startAsyncWrite() {
    if (async_write_running) {
        return false;
    }
    
    async_write_running = true;
    async_write_thread = std::thread(&PL011UART::asyncWriteLoop, this);
    
    return true;
}

void PL011UART::stopAsyncWrite() {
    if (!async_write_running) {
        return;
    }
    
    async_write_running = false;
    if (async_write_thread.joinable()) {
        async_write_thread.join();
    }
}

bool PL011UART::queueData(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    write_queue.push(data);
    return true;
}

bool PL011UART::queueData(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return queueData(bytes);
}

void PL011UART::asyncReadLoop() {
    std::vector<uint8_t> buffer(4096);
    
    while (async_read_running) {
        if (dataAvailable()) {
            size_t bytes = std::min(buffer.size(), bytesAvailable());
            auto data = readData(bytes);
            
            if (!data.empty() && data_callback) {
                data_callback(data);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void PL011UART::asyncWriteLoop() {
    while (async_write_running) {
        std::vector<uint8_t> data;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!write_queue.empty()) {
                data = write_queue.front();
                write_queue.pop();
            }
        }
        
        if (!data.empty()) {
            writeData(data);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ============================================
// BARE-METAL UART
// ============================================

bool PL011UART::setupBaremetal() {
    // Map UART registers
    uart_memory = mmap(nullptr, 0x1000, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (uart_memory == MAP_FAILED) {
        notifyError("Failed to map UART memory");
        return false;
    }
    return true;
}

void PL011UART::shutdownBaremetal() {
    if (uart_memory != nullptr) {
        munmap(uart_memory, 0x1000);
        uart_memory = nullptr;
    }
}

bool PL011UART::configureBaremetal(uint32_t baud) {
    volatile uint32_t* regs = static_cast<volatile uint32_t*>(uart_memory);
    
    // Disable UART
    regs[PL011_CR / 4] = 0;
    
    // Set baud rate (assuming 48MHz clock)
    uint32_t baud_div = 48000000 / (16 * baud);
    uint32_t ibrd = baud_div >> 6;
    uint32_t fbrd = baud_div & 0x3F;
    
    regs[PL011_IBRD / 4] = ibrd;
    regs[PL011_FBRD / 4] = fbrd;
    
    // Set line control: 8N1, enable FIFO
    regs[PL011_LCRH / 4] = PL011_LCRH_WLEN_8 | PL011_LCRH_FEN;
    
    // Enable UART: TX, RX, UARTEN
    regs[PL011_CR / 4] = PL011_CR_TXE | PL011_CR_RXE | PL011_CR_UARTEN;
    
    return true;
}

void PL011UART::writeBaremetal(const uint8_t* data, size_t length) {
    volatile uint32_t* regs = static_cast<volatile uint32_t*>(uart_memory);
    
    for (size_t i = 0; i < length; i++) {
        // Wait for TX FIFO to have space
        while (regs[PL011_FR / 4] & PL011_FR_TXFF) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        regs[PL011_DR / 4] = data[i];
    }
}

size_t PL011UART::readBaremetal(uint8_t* buffer, size_t max_length) {
    volatile uint32_t* regs = static_cast<volatile uint32_t*>(uart_memory);
    size_t read = 0;
    
    while (read < max_length && !(regs[PL011_FR / 4] & PL011_FR_RXFE)) {
        buffer[read++] = regs[PL011_DR / 4] & 0xFF;
    }
    
    return read;
}

bool PL011UART::dataAvailableBaremetal() const {
    volatile uint32_t* regs = static_cast<volatile uint32_t*>(uart_memory);
    return !(regs[PL011_FR / 4] & PL011_FR_RXFE);
}

void PL011UART::flushBaremetal() {
    volatile uint32_t* regs = static_cast<volatile uint32_t*>(uart_memory);
    while (!(regs[PL011_FR / 4] & PL011_FR_RXFE)) {
        volatile uint32_t dummy = regs[PL011_DR / 4];
        (void)dummy;
    }
}

// ============================================
// STATISTICS
// ============================================

PL011UART::UARTStats PL011UART::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void PL011UART::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = UARTStats{};
}

void PL011UART::updateStatsSent(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.bytes_sent += bytes;
    stats.last_activity = std::chrono::system_clock::now();
}

void PL011UART::updateStatsReceived(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.bytes_received += bytes;
    stats.last_activity = std::chrono::system_clock::now();
}

void PL011UART::notifyStats() {
    if (on_stats) {
        on_stats(getStats());
    }
}

bool PL011UART::isConnected() const {
    return state != UARTState::CLOSED && uart_fd >= 0;
}

// ============================================
// CONFIGURATION
// ============================================

bool PL011UART::setBaudRate(int baud_rate) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    config.baud_rate = baud_rate;
    
    if (baremetal_mode) {
        return configureBaremetal(baud_rate);
    }
    
    return configureDevice();
}

bool PL011UART::setDataBits(DataBits data_bits) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    config.data_bits = data_bits;
    return configureDevice();
}

bool PL011UART::setStopBits(StopBits stop_bits) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    config.stop_bits = stop_bits;
    return configureDevice();
}

bool PL011UART::setParity(Parity parity) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    config.parity = parity;
    return configureDevice();
}

bool PL011UART::setFlowControl(FlowControl flow_control) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    config.flow_control = flow_control;
    return configureDevice();
}

bool PL011UART::setReadTimeout(int timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    config.read_timeout_ms = timeout_ms;
    return configureDevice();
}

// ============================================
// CALLBACKS
// ============================================

void PL011UART::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void PL011UART::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void PL011UART::setOnStats(StatsCallback callback) {
    on_stats = callback;
}

void PL011UART::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "UART Error: " << error << std::endl;
}

void PL011UART::setState(UARTState new_state) {
    if (state != new_state) {
        state = new_state;
        notifyStateChange();
    }
}

void PL011UART::notifyStateChange() {
    if (on_state_change) {
        on_state_change(state);
    }
}

// ============================================
// UTILITY
// ============================================

std::string PL011UART::getStateName() const {
    switch (state) {
        case UARTState::CLOSED: return "CLOSED";
        case UARTState::OPEN: return "OPEN";
        case UARTState::CONFIGURED: return "CONFIGURED";
        case UARTState::RUNNING: return "RUNNING";
        case UARTState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::vector<PL011UART::DeviceInfo> PL011UART::listDevices() const {
    std::vector<DeviceInfo> devices;
    
    // Check common UART devices
    std::vector<std::string> uart_devices = {
        "/dev/ttyAMA0", "/dev/ttyAMA1", "/dev/ttyAMA2", "/dev/ttyAMA3",
        "/dev/ttyAMA4", "/dev/ttyAMA5", "/dev/ttyAMA6", "/dev/ttyAMA7",
        "/dev/ttyAMA8", "/dev/ttyAMA9", "/dev/ttyAMA10",
        "/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2"
    };
    
    for (const auto& dev : uart_devices) {
        DeviceInfo info;
        info.device_path = dev;
        info.is_available = false;
        info.is_pl011 = false;
        info.supported_baud_rates = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
        info.supported_data_bits = {DataBits::SEVEN, DataBits::EIGHT};
        
        // Check if device exists
        if (access(dev.c_str(), F_OK) == 0) {
            info.is_available = true;
            // Check if it's PL011 (try to open and get driver info)
            int fd = open(dev.c_str(), O_RDWR | O_NOCTTY);
            if (fd >= 0) {
                struct termios options;
                if (tcgetattr(fd, &options) == 0) {
                    info.is_pl011 = true;
                }
                close(fd);
            }
        }
        
        devices.push_back(info);
    }
    
    return devices;
}

bool PL011UART::testLoopback(const std::vector<uint8_t>& test_data) {
    if (state == UARTState::CLOSED) {
        return false;
    }
    
    // Send test data
    if (!writeData(test_data)) {
        return false;
    }
    
    // Wait for data to be sent and received
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Read back
    auto received = readData(test_data.size());
    
    // Compare
    if (received.size() != test_data.size()) {
        return false;
    }
    
    return std::equal(test_data.begin(), test_data.end(), received.begin());
}

int PL011UART::getBaudFlag(int baud) const {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default: return B115200;
    }
}

std::string PL011UART::getBaudRateName(int baud) const {
    return std::to_string(baud) + " baud";
}

std::string PL011UART::getParityName(Parity parity) const {
    switch (parity) {
        case Parity::NONE: return "None";
        case Parity::EVEN: return "Even";
        case Parity::ODD: return "Odd";
        case Parity::MARK: return "Mark";
        case Parity::SPACE: return "Space";
        default: return "Unknown";
    }
}

std::string PL011UART::getStopBitsName(StopBits stop_bits) const {
    switch (stop_bits) {
        case StopBits::ONE: return "1";
        case StopBits::TWO: return "2";
        default: return "Unknown";
    }
}

std::string PL011UART::getDataBitsName(DataBits data_bits) const {
    switch (data_bits) {
        case DataBits::FIVE: return "5";
        case DataBits::SIX: return "6";
        case DataBits::SEVEN: return "7";
        case DataBits::EIGHT: return "8";
        default: return "Unknown";
    }
}

std::string PL011UART::getFlowControlName(FlowControl flow_control) const {
    switch (flow_control) {
        case FlowControl::NONE: return "None";
        case FlowControl::RTS_CTS: return "RTS/CTS";
        case FlowControl::XON_XOFF: return "XON/XOFF";
        default: return "Unknown";
    }
}

bool PL011UART::sendBreak(int duration_ms) {
    if (uart_fd < 0) {
        return false;
    }
    return tcsendbreak(uart_fd, duration_ms) == 0;
}

bool PL011UART::setDTR(bool enable) {
    if (uart_fd < 0) {
        return false;
    }
    int status;
    if (ioctl(uart_fd, TIOCMGET, &status) < 0) {
        return false;
    }
    if (enable) {
        status |= TIOCM_DTR;
    } else {
        status &= ~TIOCM_DTR;
    }
    return ioctl(uart_fd, TIOCMSET, &status) == 0;
}

bool PL011UART::setRTS(bool enable) {
    if (uart_fd < 0) {
        return false;
    }
    int status;
    if (ioctl(uart_fd, TIOCMGET, &status) < 0) {
        return false;
    }
    if (enable) {
        status |= TIOCM_RTS;
    } else {
        status &= ~TIOCM_RTS;
    }
    return ioctl(uart_fd, TIOCMSET, &status) == 0;
}

bool PL011UART::getDSR() const {
    if (uart_fd < 0) {
        return false;
    }
    int status;
    if (ioctl(uart_fd, TIOCMGET, &status) < 0) {
        return false;
    }
    return status & TIOCM_DSR;
}

bool PL011UART::getCTS() const {
    if (uart_fd < 0) {
        return false;
    }
    int status;
    if (ioctl(uart_fd, TIOCMGET, &status) < 0) {
        return false;
    }
    return status & TIOCM_CTS;
}

bool PL011UART::getDCD() const {
    if (uart_fd < 0) {
        return false;
    }
    int status;
    if (ioctl(uart_fd, TIOCMGET, &status) < 0) {
        return false;
    }
    return status & TIOCM_CD;
}

bool PL011UART::getRI() const {
    if (uart_fd < 0) {
        return false;
    }
    int status;
    if (ioctl(uart_fd, TIOCMGET, &status) < 0) {
        return false;
    }
    return status & TIOCM_RI;
}

void PL011UART::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

std::string PL011UART::getCurrentTimeStr() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buffer[64];
    struct tm* tm_info = localtime(&time_t_now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}
