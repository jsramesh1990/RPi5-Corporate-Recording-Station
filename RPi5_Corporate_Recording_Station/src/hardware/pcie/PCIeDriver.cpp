#include "PCIeDriver.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <regex>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <errno.h>

// Device database (common PCIe devices)
const std::vector<PCIeDriver::VendorDevice> PCIeDriver::device_database = {
    // NVMe Controllers
    {0x144D, 0xA808, "Samsung", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x144D, 0xA809, "Samsung", "NVMe SSD Controller (PM9A1)", DeviceType::NVME_SSD},
    {0x144D, 0xA80A, "Samsung", "NVMe SSD Controller (PM981)", DeviceType::NVME_SSD},
    {0x144D, 0xA80B, "Samsung", "NVMe SSD Controller (PM991)", DeviceType::NVME_SSD},
    {0x1E0F, 0x0001, "Kioxia", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1E0F, 0x0002, "Kioxia", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1E0F, 0x0003, "Kioxia", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x15B7, 0x5003, "Sandisk", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x15B7, 0x5006, "Sandisk", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x15B7, 0x5009, "Sandisk", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x15B7, 0x5011, "Sandisk", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x15B7, 0x5015, "Sandisk", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x10EC, 0x5762, "Realtek", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x10EC, 0x5763, "Realtek", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x126F, 0x2262, "Silicon Motion", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x126F, 0x2263, "Silicon Motion", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1179, 0x010F, "Toshiba", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1179, 0x0115, "Toshiba", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1B4B, 0x9210, "Marvell", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1B4B, 0x9215, "Marvell", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1B4B, 0x9230, "Marvell", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1B4B, 0x9235, "Marvell", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x8086, 0xF1A8, "Intel", "NVMe SSD Controller (660p)", DeviceType::NVME_SSD},
    {0x8086, 0xF1A6, "Intel", "NVMe SSD Controller (760p)", DeviceType::NVME_SSD},
    {0x8086, 0x0975, "Intel", "NVMe SSD Controller (905P)", DeviceType::NVME_SSD},
    {0x1CC1, 0x8201, "ADATA", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1CC1, 0x8202, "ADATA", "NVMe SSD Controller", DeviceType::NVME_SSD},
    {0x1CC1, 0x8203, "ADATA", "NVMe SSD Controller", DeviceType::NVME_SSD},
    
    // SATA Controllers
    {0x1B21, 0x0612, "ASMedia", "SATA 6G Controller", DeviceType::SATA_CONTROLLER},
    {0x1B21, 0x0625, "ASMedia", "SATA 6G Controller", DeviceType::SATA_CONTROLLER},
    {0x1B21, 0x1042, "ASMedia", "SATA 6G Controller", DeviceType::SATA_CONTROLLER},
    {0x197B, 0x2362, "JMicron", "SATA Controller", DeviceType::SATA_CONTROLLER},
    {0x197B, 0x2363, "JMicron", "SATA Controller", DeviceType::SATA_CONTROLLER},
    {0x197B, 0x2365, "JMicron", "SATA Controller", DeviceType::SATA_CONTROLLER},
    {0x197B, 0x2368, "JMicron", "SATA Controller", DeviceType::SATA_CONTROLLER},
    {0x1B4B, 0x9170, "Marvell", "SATA Controller", DeviceType::SATA_CONTROLLER},
    {0x1B4B, 0x9172, "Marvell", "SATA Controller", DeviceType::SATA_CONTROLLER},
    {0x1B4B, 0x9182, "Marvell", "SATA Controller", DeviceType::SATA_CONTROLLER},
    
    // USB Controllers
    {0x1B21, 0x1142, "ASMedia", "USB 3.1 Controller", DeviceType::USB_CONTROLLER},
    {0x1B21, 0x1242, "ASMedia", "USB 3.1 Controller", DeviceType::USB_CONTROLLER},
    {0x1B21, 0x1343, "ASMedia", "USB 3.2 Controller", DeviceType::USB_CONTROLLER},
    {0x1912, 0x0014, "Renesas", "USB 3.0 Controller", DeviceType::USB_CONTROLLER},
    {0x1912, 0x0015, "Renesas", "USB 3.0 Controller", DeviceType::USB_CONTROLLER},
    
    // Network Controllers
    {0x10EC, 0x8168, "Realtek", "Gigabit Ethernet", DeviceType::NETWORK_CONTROLLER},
    {0x10EC, 0x8169, "Realtek", "Gigabit Ethernet", DeviceType::NETWORK_CONTROLLER},
    {0x10EC, 0x8125, "Realtek", "2.5G Ethernet", DeviceType::NETWORK_CONTROLLER},
    {0x8086, 0x1533, "Intel", "Gigabit Ethernet (I210)", DeviceType::NETWORK_CONTROLLER},
    {0x8086, 0x1570, "Intel", "10G Ethernet (X710)", DeviceType::NETWORK_CONTROLLER},
    {0x8086, 0x15B8, "Intel", "Gigabit Ethernet (I219)", DeviceType::NETWORK_CONTROLLER},
    
    // PCIe Bridges
    {0x1B21, 0x1080, "ASMedia", "PCIe to PCI Bridge", DeviceType::PCIE_BRIDGE},
    {0x1B21, 0x1100, "ASMedia", "PCIe to PCI Bridge", DeviceType::PCIE_BRIDGE},
    {0x1B21, 0x1184, "ASMedia", "PCIe to PCI Bridge", DeviceType::PCIE_BRIDGE},
    {0x1B21, 0x1187, "ASMedia", "PCIe to PCI Bridge", DeviceType::PCIE_BRIDGE},
};

// ============================================
// SINGLETON
// ============================================

PCIeDriver& PCIeDriver::getInstance() {
    static PCIeDriver instance;
    return instance;
}

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

PCIeDriver::PCIeDriver() {
    // Initialize with default bus
    initialize();
}

PCIeDriver::~PCIeDriver() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool PCIeDriver::initialize() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }
    
    // Check if PCIe is available
    if (!isPCIeAvailable()) {
        std::cerr << "PCIe not available on this system" << std::endl;
        return false;
    }
    
    // Scan for devices
    scanPCIeBus();
    scanNVMeDevices();
    
    initialized = true;
    std::cout << "PCIe Driver initialized" << std::endl;
    return true;
}

void PCIeDriver::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Unmount any mounted devices
    for (const auto& [path, device] : devices) {
        if (device.is_active && isMounted(path)) {
            unmountNVMe(path);
        }
    }
    
    devices.clear();
    device_stats.clear();
    nvme_devices.clear();
    
    initialized = false;
    std::cout << "PCIe Driver shutdown" << std::endl;
}

// ============================================
// DEVICE MANAGEMENT
// ============================================

std::vector<PCIeDriver::DeviceInfo> PCIeDriver::scanDevices() {
    std::lock_guard<std::mutex> lock(mutex);
    
    scanPCIeBus();
    scanNVMeDevices();
    
    std::vector<DeviceInfo> result;
    for (const auto& [path, device] : devices) {
        result.push_back(device);
    }
    return result;
}

PCIeDriver::DeviceInfo PCIeDriver::getDeviceInfo(uint8_t bus, uint8_t slot, uint8_t function) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::string path = getSysPath(bus, slot, function);
    auto it = devices.find(path);
    if (it != devices.end()) {
        return it->second;
    }
    return DeviceInfo{};
}

PCIeDriver::DeviceInfo PCIeDriver::getDeviceInfo(const std::string& device_path) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    for (const auto& [path, device] : devices) {
        if (device.device_path == device_path) {
            return device;
        }
    }
    return DeviceInfo{};
}

std::vector<PCIeDriver::DeviceInfo> PCIeDriver::getAllDevices() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<DeviceInfo> result;
    for (const auto& [path, device] : devices) {
        result.push_back(device);
    }
    return result;
}

std::vector<PCIeDriver::DeviceInfo> PCIeDriver::getDevicesByType(DeviceType type) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<DeviceInfo> result;
    for (const auto& [path, device] : devices) {
        if (device.type == type) {
            result.push_back(device);
        }
    }
    return result;
}

std::vector<PCIeDriver::NVMeInfo> PCIeDriver::getNVMeDevices() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<NVMeInfo> result;
    for (const auto& [path, info] : nvme_devices) {
        result.push_back(info);
    }
    return result;
}

PCIeDriver::NVMeInfo PCIeDriver::getNVMeDevice(const std::string& device_path) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = nvme_devices.find(device_path);
    if (it != nvme_devices.end()) {
        return it->second;
    }
    return NVMeInfo{};
}

// ============================================
// SCAN FUNCTIONS
// ============================================

void PCIeDriver::scanPCIeBus() {
    std::string pci_path = getPCIBusPath();
    if (pci_path.empty()) {
        return;
    }
    
    // List all PCIe devices
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                DeviceInfo info = scanPCIDevice(bus, slot, func);
                if (info.vendor_id != 0 && info.device_id != 0) {
                    devices[getSysPath(bus, slot, func)] = info;
                    notifyDeviceDetected(info);
                }
            }
        }
    }
}

PCIeDriver::DeviceInfo PCIeDriver::scanPCIDevice(uint8_t bus, uint8_t slot, uint8_t function) {
    DeviceInfo info;
    info.bus = bus;
    info.slot = slot;
    info.function = function;
    
    std::string path = getSysPath(bus, slot, function);
    
    // Read vendor ID
    std::string vendor_path = path + "/vendor";
    std::string vendor_content;
    if (readFile(vendor_path, vendor_content)) {
        info.vendor_id = std::stoul(vendor_content, nullptr, 16);
    }
    
    // Read device ID
    std::string device_path = path + "/device";
    std::string device_content;
    if (readFile(device_path, device_content)) {
        info.device_id = std::stoul(device_content, nullptr, 16);
    }
    
    // If vendor and device IDs are valid, continue
    if (info.vendor_id == 0 || info.device_id == 0) {
        return info;
    }
    
    // Determine device type
    info.type = determineDeviceType(info.vendor_id, info.device_id);
    info.vendor_name = getVendorName(info.vendor_id);
    info.device_name = getDeviceName(info.vendor_id, info.device_id);
    
    // Read speed
    std::string speed_path = path + "/max_link_speed";
    std::string speed_content;
    if (readFile(speed_path, speed_content)) {
        int speed_value = std::stoi(speed_content);
        switch (speed_value) {
            case 1: info.speed = Speed::GEN1; break;
            case 2: info.speed = Speed::GEN2; break;
            case 3: info.speed = Speed::GEN3; break;
            case 4: info.speed = Speed::GEN4; break;
            case 5: info.speed = Speed::GEN5; break;
            default: info.speed = Speed::GEN1; break;
        }
    }
    
    // Read driver
    std::string driver_path = path + "/driver";
    if (pathExists(driver_path)) {
        std::string driver_link;
        // Read symlink target
        char buffer[256];
        ssize_t len = readlink(driver_path.c_str(), buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';
            info.driver = std::string(buffer);
            size_t pos = info.driver.find_last_of('/');
            if (pos != std::string::npos) {
                info.driver = info.driver.substr(pos + 1);
            }
        }
    }
    
    // Read BAR addresses
    for (int i = 0; i < 6; i++) {
        std::string bar_path = path + "/resource" + std::to_string(i);
        std::string bar_content;
        if (readFile(bar_path, bar_content)) {
            std::stringstream ss(bar_content);
            uint32_t start, end, flags;
            ss >> std::hex >> start >> std::hex >> end >> std::hex >> flags;
            if (start != 0 || end != 0) {
                info.bar_addresses.push_back(start);
                info.bar_sizes.push_back(end - start + 1);
            }
        }
    }
    
    // Set device path
    if (info.type == DeviceType::NVME_SSD) {
        // Try to find NVMe device path
        auto nvme_devices = getNVMeDevicesFromSys();
        for (const auto& dev : nvme_devices) {
            // Check if this NVMe device corresponds to this PCIe device
            // This is a simplified mapping
            info.device_path = "/dev/" + dev;
            break;
        }
    }
    
    info.state = DeviceState::DETECTED;
    info.is_active = true;
    
    return info;
}

void PCIeDriver::scanNVMeDevices() {
    auto devices_list = getNVMeDevicesFromSys();
    
    for (const auto& dev : devices_list) {
        std::string dev_path = "/dev/" + dev;
        NVMeInfo info;
        info.is_nvme = true;
        info.device_path = dev_path;
        
        parseNVMeInfo(dev_path, info);
        nvme_devices[dev_path] = info;
        
        // Update device info
        for (auto& [path, device] : devices) {
            if (device.device_path == dev_path) {
                device.state = DeviceState::INITIALIZED;
                device.is_active = true;
                break;
            }
        }
    }
}

// ============================================
// NVMe OPERATIONS
// ============================================

bool PCIeDriver::initializeNVMe(const std::string& device_path) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = nvme_devices.find(device_path);
    if (it == nvme_devices.end()) {
        // Try to detect
        parseNVMeInfo(device_path, it->second);
        if (!it->second.is_nvme) {
            std::cerr << "Device is not NVMe: " << device_path << std::endl;
            return false;
        }
    }
    
    // Update device info
    for (auto& [path, device] : devices) {
        if (device.device_path == device_path) {
            device.state = DeviceState::INITIALIZED;
            device.is_active = true;
            break;
        }
    }
    
    return true;
}

bool PCIeDriver::mountNVMe(const std::string& device_path,
                           const std::string& mount_point,
                           const std::string& filesystem) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!pathExists(device_path)) {
        std::cerr << "Device does not exist: " << device_path << std::endl;
        return false;
    }
    
    // Create mount point if it doesn't exist
    if (!pathExists(mount_point)) {
        if (mkdir(mount_point.c_str(), 0755) != 0) {
            std::cerr << "Failed to create mount point: " << mount_point << std::endl;
            return false;
        }
    }
    
    // Check if already mounted
    if (isMounted(device_path)) {
        return true;
    }
    
    // Mount device
    int flags = MS_NOATIME;
    if (mount(device_path.c_str(), mount_point.c_str(), filesystem.c_str(), flags, nullptr) != 0) {
        std::cerr << "Failed to mount: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Update NVMe info
    auto it = nvme_devices.find(device_path);
    if (it != nvme_devices.end()) {
        it->second.is_mounted = true;
        it->second.mount_point = mount_point;
        it->second.filesystem = filesystem;
        
        // Get capacity info
        struct statvfs stat;
        if (statvfs(mount_point.c_str(), &stat) == 0) {
            it->second.total_capacity = stat.f_blocks * stat.f_frsize;
            it->second.free_capacity = stat.f_bfree * stat.f_frsize;
            it->second.used_capacity = it->second.total_capacity - it->second.free_capacity;
        }
    }
    
    std::cout << "Mounted " << device_path << " at " << mount_point << std::endl;
    return true;
}

bool PCIeDriver::unmountNVMe(const std::string& device_path) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = nvme_devices.find(device_path);
    if (it == nvme_devices.end()) {
        return false;
    }
    
    if (!it->second.is_mounted) {
        return true;
    }
    
    if (umount(it->second.mount_point.c_str()) != 0) {
        std::cerr << "Failed to unmount: " << strerror(errno) << std::endl;
        return false;
    }
    
    it->second.is_mounted = false;
    it->second.mount_point = "";
    
    std::cout << "Unmounted " << device_path << std::endl;
    return true;
}

bool PCIeDriver::formatNVMe(const std::string& device_path,
                            const std::string& filesystem,
                            const std::string& label) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!pathExists(device_path)) {
        std::cerr << "Device does not exist: " << device_path << std::endl;
        return false;
    }
    
    // Unmount if mounted
    if (isMounted(device_path)) {
        unmountNVMe(device_path);
    }
    
    // Build command
    std::string command = "mkfs." + filesystem;
    if (!label.empty()) {
        command += " -L " + label;
    }
    command += " " + device_path + " -f";
    
    std::string output;
    if (!executeCommand(command, output)) {
        std::cerr << "Failed to format device: " << output << std::endl;
        return false;
    }
    
    std::cout << "Formatted " << device_path << " with " << filesystem << std::endl;
    return true;
}

bool PCIeDriver::createPartition(const std::string& device_path, uint64_t partition_size) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!pathExists(device_path)) {
        std::cerr << "Device does not exist: " << device_path << std::endl;
        return false;
    }
    
    // Unmount if mounted
    if (isMounted(device_path)) {
        unmountNVMe(device_path);
    }
    
    // Use parted to create partition
    std::string command = "parted -s " + device_path + " mklabel gpt";
    std::string output;
    if (!executeCommand(command, output)) {
        std::cerr << "Failed to create partition table: " << output << std::endl;
        return false;
    }
    
    // Calculate partition size
    std::string size_str = partition_size > 0 ? 
        std::to_string(partition_size / 1024 / 1024) + "MB" : "100%";
    
    command = "parted -s " + device_path + " mkpart primary 1 " + size_str;
    if (!executeCommand(command, output)) {
        std::cerr << "Failed to create partition: " << output << std::endl;
        return false;
    }
    
    std::cout << "Created partition on " << device_path << std::endl;
    return true;
}

// ============================================
// PERFORMANCE
// ============================================

PCIeDriver::SpeedTestResult PCIeDriver::runSpeedTest(const std::string& device_path, size_t size_mb) {
    SpeedTestResult result;
    
    std::cout << "Running speed test on " << device_path << " (" << size_mb << "MB)..." << std::endl;
    
    // Create a temporary file for testing
    std::string temp_file = "/tmp/speed_test_" + std::to_string(time(nullptr));
    
    // Write test
    auto start = std::chrono::steady_clock::now();
    std::string write_cmd = "dd if=/dev/zero of=" + temp_file + 
                           " bs=1M count=" + std::to_string(size_mb) + 
                           " conv=fdatasync 2>&1 | grep -E 'MB/s|GB/s'";
    std::string write_output;
    if (executeCommand(write_cmd, write_output)) {
        // Parse speed from output
        std::regex speed_regex(R"((\d+\.?\d*)\s+(MB|GB)/s)");
        std::smatch match;
        if (std::regex_search(write_output, match, speed_regex)) {
            double speed = std::stod(match[1].str());
            if (match[2].str() == "GB") {
                speed *= 1024;
            }
            result.write_speed_mb_s = speed;
        }
    }
    
    // Read test
    start = std::chrono::steady_clock::now();
    std::string read_cmd = "dd if=" + temp_file + 
                          " of=/dev/null bs=1M 2>&1 | grep -E 'MB/s|GB/s'";
    std::string read_output;
    if (executeCommand(read_cmd, read_output)) {
        std::regex speed_regex(R"((\d+\.?\d*)\s+(MB|GB)/s)");
        std::smatch match;
        if (std::regex_search(read_output, match, speed_regex)) {
            double speed = std::stod(match[1].str());
            if (match[2].str() == "GB") {
                speed *= 1024;
            }
            result.read_speed_mb_s = speed;
        }
    }
    
    // Clean up
    std::string rm_cmd = "rm -f " + temp_file;
    executeCommand(rm_cmd, read_output);
    
    // Get IOPS
    std::string iops_cmd = "fio --name=randwrite --ioengine=libaio --rw=randwrite " +
                          "--bs=4k --size=" + std::to_string(size_mb) + "M " +
                          "--numjobs=1 --runtime=30 --time_based " +
                          "--filename=" + temp_file + " 2>&1 | grep -E 'IOPS'";
    std::string iops_output;
    if (executeCommand(iops_cmd, iops_output)) {
        std::regex iops_regex(R"((\d+)\s*IOPS)");
        std::smatch match;
        if (std::regex_search(iops_output, match, iops_regex)) {
            result.random_write_iops = std::stod(match[1].str());
        }
    }
    
    // Clean up
    executeCommand("rm -f " + temp_file, read_output);
    
    std::cout << "Speed test complete: Read " << result.read_speed_mb_s 
              << " MB/s, Write " << result.write_speed_mb_s << " MB/s" << std::endl;
    
    return result;
}

PCIeDriver::BenchmarkResult PCIeDriver::benchmark(const std::string& device_path, int iterations) {
    BenchmarkResult result;
    
    std::vector<double> read_speeds;
    std::vector<double> write_speeds;
    std::vector<double> random_read_speeds;
    std::vector<double> random_write_speeds;
    std::vector<double> latencies;
    
    for (int i = 0; i < iterations; i++) {
        auto speed_result = runSpeedTest(device_path, 1024);
        read_speeds.push_back(speed_result.read_speed_mb_s);
        write_speeds.push_back(speed_result.write_speed_mb_s);
        
        // Random tests
        std::string temp_file = "/tmp/random_test_" + std::to_string(i);
        std::string random_read_cmd = "fio --name=randread --ioengine=libaio --rw=randread " +
                                     "--bs=4k --size=1G --numjobs=1 --runtime=30 --time_based " +
                                     "--filename=" + temp_file + " 2>&1 | grep -E 'BW=' | head -1";
        std::string output;
        if (executeCommand(random_read_cmd, output)) {
            std::regex bw_regex(R"(BW=(\d+\.?\d*)([KM])B/s)");
            std::smatch match;
            if (std::regex_search(output, match, bw_regex)) {
                double speed = std::stod(match[1].str());
                if (match[2].str() == "K") {
                    speed /= 1024;
                }
                random_read_speeds.push_back(speed);
            }
        }
        
        // Clean up
        executeCommand("rm -f " + temp_file, output);
    }
    
    // Calculate results
    result.sequential_read_mb_s = calculateAverage(read_speeds);
    result.sequential_write_mb_s = calculateAverage(write_speeds);
    result.random_read_mb_s = calculateAverage(random_read_speeds);
    result.random_write_mb_s = calculateAverage(random_write_speeds);
    result.mixed_workload_mb_s = (result.sequential_read_mb_s + result.sequential_write_mb_s) / 2;
    result.iops_read = result.random_read_mb_s * 1024 * 1024 / 4096;
    result.iops_write = result.random_write_mb_s * 1024 * 1024 / 4096;
    
    return result;
}

// ============================================
// STATISTICS
// ============================================

PCIeDriver::PCIeStats PCIeDriver::getStats(const std::string& device_path) const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    
    auto it = device_stats.find(device_path);
    if (it != device_stats.end()) {
        return it->second;
    }
    return PCIeStats{};
}

PCIeDriver::PCIeStats PCIeDriver::getTotalStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    
    PCIeStats total;
    for (const auto& [path, stats] : device_stats) {
        total.total_bytes_read += stats.total_bytes_read;
        total.total_bytes_written += stats.total_bytes_written;
        total.total_transfers += stats.total_transfers;
        total.read_operations += stats.read_operations;
        total.write_operations += stats.write_operations;
        total.read_errors += stats.read_errors;
        total.write_errors += stats.write_errors;
        total.retry_count += stats.retry_count;
        total.power_on_hours += stats.power_on_hours;
    }
    return total;
}

void PCIeDriver::resetStats(const std::string& device_path) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    device_stats[device_path] = PCIeStats{};
}

void PCIeDriver::resetAllStats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    for (auto& [path, stats] : device_stats) {
        stats = PCIeStats{};
    }
}

// ============================================
// NVME INFO PARSING
// ============================================

void PCIeDriver::parseNVMeInfo(const std::string& device_path, NVMeInfo& info) {
    // Get device info using nvme-cli if available
    std::string nvme_cmd = "nvme list -o json 2>/dev/null | grep -A 10 " + device_path;
    std::string output;
    if (executeCommand(nvme_cmd, output)) {
        // Parse JSON output (simplified)
        std::regex model_regex(R"("ModelNumber"\s*:\s*"([^"]+)")");
        std::regex serial_regex(R"("SerialNumber"\s*:\s*"([^"]+)")");
        std::regex fw_regex(R"("Firmware"\s*:\s*"([^"]+)")");
        
        std::smatch match;
        if (std::regex_search(output, match, model_regex)) {
            info.model = match[1].str();
        }
        if (std::regex_search(output, match, serial_regex)) {
            info.serial_number = match[1].str();
        }
        if (std::regex_search(output, match, fw_regex)) {
            info.firmware_version = match[1].str();
        }
    }
    
    // Get capacity using lsblk
    std::string lsblk_cmd = "lsblk -b -n -o SIZE " + device_path + " 2>/dev/null";
    if (executeCommand(lsblk_cmd, output)) {
        try {
            info.total_capacity = std::stoull(output);
        } catch (...) {
            // Ignore parsing errors
        }
    }
    
    // Get SMART data
    std::string smart_cmd = "nvme smart-log " + device_path + " 2>/dev/null | grep -E 'temperature|percentage|power_on'";
    if (executeCommand(smart_cmd, output)) {
        std::regex temp_regex(R"(temperature\s*:\s*(\d+))");
        std::regex pct_regex(R"(percentage_used\s*:\s*(\d+))");
        std::regex power_regex(R"(power_on_hours\s*:\s*(\d+))");
        
        std::smatch match;
        if (std::regex_search(output, match, temp_regex)) {
            info.temperature_celsius = std::stoul(match[1].str());
        }
        if (std::regex_search(output, match, pct_regex)) {
            info.percentage_used = std::stoul(match[1].str());
        }
        if (std::regex_search(output, match, power_regex)) {
            info.power_on_hours = std::stoull(match[1].str());
        }
    }
    
    info.is_nvme = true;
}

// ============================================
// PRIVATE METHODS
// ============================================

PCIeDriver::DeviceType PCIeDriver::determineDeviceType(uint16_t vendor_id, uint16_t device_id) {
    for (const auto& dev : device_database) {
        if (dev.vendor_id == vendor_id && dev.device_id == device_id) {
            return dev.type;
        }
    }
    return DeviceType::UNKNOWN;
}

std::string PCIeDriver::getVendorName(uint16_t vendor_id) const {
    for (const auto& dev : device_database) {
        if (dev.vendor_id == vendor_id) {
            return dev.vendor_name;
        }
    }
    return "Unknown Vendor";
}

std::string PCIeDriver::getDeviceName(uint16_t vendor_id, uint16_t device_id) const {
    for (const auto& dev : device_database) {
        if (dev.vendor_id == vendor_id && dev.device_id == device_id) {
            return dev.device_name;
        }
    }
    return "Unknown Device";
}

bool PCIeDriver::executeCommand(const std::string& command, std::string& output) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return false;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int status = pclose(pipe);
    return status == 0;
}

bool PCIeDriver::pathExists(const std::string& path) const {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool PCIeDriver::readFile(const std::string& path, std::string& content) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::getline(file, content);
    file.close();
    return true;
}

bool PCIeDriver::writeFile(const std::string& path, const std::string& content) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    file.close();
    return true;
}

std::vector<std::string> PCIeDriver::listDirectory(const std::string& path) const {
    std::vector<std::string> files;
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        return files;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
            files.push_back(entry->d_name);
        }
    }
    closedir(dir);
    return files;
}

std::string PCIeDriver::getSysPath(uint8_t bus, uint8_t slot, uint8_t function) const {
    return "/sys/bus/pci/devices/" + std::to_string(bus) + ":" + 
           std::to_string(slot) + "." + std::to_string(function);
}

std::string PCIeDriver::getPCIBusPath() const {
    return "/sys/bus/pci/devices/";
}

std::string PCIeDriver::getNVMePath(const std::string& device) const {
    return "/dev/" + device;
}

std::vector<std::string> PCIeDriver::getBlockDevices() const {
    std::vector<std::string> devices;
    std::string path = "/sys/block/";
    auto entries = listDirectory(path);
    for (const auto& entry : entries) {
        // Check if it's an NVMe device
        std::string dev_path = path + entry;
        if (pathExists(dev_path + "/device/")) {
            devices.push_back(entry);
        }
    }
    return devices;
}

std::vector<std::string> PCIeDriver::getNVMeDevicesFromSys() const {
    std::vector<std::string> nvme_devices;
    std::string path = "/sys/block/";
    auto entries = listDirectory(path);
    for (const auto& entry : entries) {
        // Check if it's an NVMe device (starts with "nvme")
        if (entry.substr(0, 4) == "nvme") {
            nvme_devices.push_back(entry);
        }
    }
    return nvme_devices;
}

bool PCIeDriver::isPCIeAvailable() const {
    return pathExists("/sys/bus/pci/");
}

bool PCIeDriver::isNVMe(const std::string& device_path) const {
    return device_path.substr(0, 4) == "/dev/nvme" ||
           device_path.find("nvme") != std::string::npos;
}

bool PCIeDriver::isMounted(const std::string& device_path) const {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(device_path) == 0) {
            return true;
        }
    }
    return false;
}

std::string PCIeDriver::getMountPoint(const std::string& device_path) const {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(device_path) == 0) {
            size_t pos = line.find(' ', device_path.length() + 1);
            if (pos != std::string::npos) {
                return line.substr(device_path.length() + 1, pos - device_path.length() - 1);
            }
        }
    }
    return "";
}

std::string PCIeDriver::getFilesystem(const std::string& device_path) const {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(device_path) == 0) {
            std::vector<std::string> parts;
            std::stringstream ss(line);
            std::string part;
            while (std::getline(ss, part, ' ')) {
                parts.push_back(part);
            }
            if (parts.size() >= 3) {
                return parts[2];
            }
        }
    }
    return "";
}

bool PCIeDriver::verifyHealth(const std::string& device_path) const {
    auto it = nvme_devices.find(device_path);
    if (it != nvme_devices.end()) {
        // Check health metrics
        if (it->second.percentage_used > 90) {
            return false;
        }
        if (it->second.temperature_celsius > 85) {
            return false;
        }
        return true;
    }
    return false;
}

PCIeDriver::DiagnosticResult PCIeDriver::runDiagnostics(const std::string& device_path) {
    DiagnosticResult result;
    result.device_path = device_path;
    result.passed = true;
    
    // Check if device exists
    if (!pathExists(device_path)) {
        result.passed = false;
        result.error_message = "Device does not exist";
        return result;
    }
    
    // Run smartctl if available
    std::string output;
    std::string smart_cmd = "smartctl -H " + device_path + " 2>/dev/null";
    if (executeCommand(smart_cmd, output)) {
        result.tests["smart"] = output.find("PASSED") != std::string::npos;
        result.details["smart"] = output;
    }
    
    // Check if device is NVMe
    if (isNVMe(device_path)) {
        // Run nvme-cli if available
        std::string nvme_cmd = "nvme smart-log " + device_path + " 2>/dev/null";
        if (executeCommand(nvme_cmd, output)) {
            result.tests["nvme"] = output.find("ERROR") == std::string::npos;
            result.details["nvme"] = output;
        }
    }
    
    // Check temperature
    if (getNVMeTemperature(device_path) > 85) {
        result.passed = false;
        result.tests["temperature"] = false;
    } else {
        result.tests["temperature"] = true;
    }
    
    // Check mount status
    result.tests["mounted"] = isMounted(device_path);
    
    // Check read/write access
    std::string test_file = device_path + "/test_write";
    if (writeFile(test_file, "test")) {
        result.tests["write"] = true;
        // Clean up
        std::string rm_cmd = "rm -f " + test_file;
        executeCommand(rm_cmd, output);
    } else {
        result.tests["write"] = false;
        result.passed = false;
    }
    
    return result;
}

// ============================================
// CALLBACKS
// ============================================

void PCIeDriver::setOnDeviceDetected(DeviceDetectedCallback callback) {
    on_device_detected = callback;
}

void PCIeDriver::setOnDeviceRemoved(DeviceRemovedCallback callback) {
    on_device_removed = callback;
}

void PCIeDriver::setOnDeviceChanged(DeviceChangedCallback callback) {
    on_device_changed = callback;
}

void PCIeDriver::setOnError(ErrorCallback callback) {
    on_error = callback;
}

void PCIeDriver::setOnSpeedTest(SpeedTestCallback callback) {
    on_speed_test = callback;
}

// ============================================
// UTILITY
// ============================================

std::string PCIeDriver::getDeviceTypeName(DeviceType type) const {
    switch (type) {
        case DeviceType::NVME_SSD: return "NVMe SSD";
        case DeviceType::SATA_CONTROLLER: return "SATA Controller";
        case DeviceType::USB_CONTROLLER: return "USB Controller";
        case DeviceType::NETWORK_CONTROLLER: return "Network Controller";
        case DeviceType::AUDIO_CONTROLLER: return "Audio Controller";
        case DeviceType::VIDEO_CONTROLLER: return "Video Controller";
        case DeviceType::PCIE_BRIDGE: return "PCIe Bridge";
        default: return "Unknown Device";
    }
}

std::string PCIeDriver::getSpeedName(Speed speed) const {
    switch (speed) {
        case Speed::GEN1: return "Gen1 (2.5 GT/s)";
        case Speed::GEN2: return "Gen2 (5 GT/s)";
        case Speed::GEN3: return "Gen3 (8 GT/s)";
        case Speed::GEN4: return "Gen4 (16 GT/s)";
        case Speed::GEN5: return "Gen5 (32 GT/s)";
        default: return "Unknown Speed";
    }
}

std::string PCIeDriver::getStateName(DeviceState state) const {
    switch (state) {
        case DeviceState::UNKNOWN: return "Unknown";
        case DeviceState::DETECTED: return "Detected";
        case DeviceState::INITIALIZED: return "Initialized";
        case DeviceState::ACTIVE: return "Active";
        case DeviceState::ERROR: return "Error";
        case DeviceState::REMOVED: return "Removed";
        default: return "Unknown";
    }
}

std::string PCIeDriver::formatCapacity(uint64_t bytes) const {
    return formatSize(bytes);
}

std::string PCIeDriver::formatSize(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int i = 0;
    double size = bytes;
    while (size >= 1024 && i < 5) {
        size /= 1024;
        i++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[i];
    return ss.str();
}

double PCIeDriver::calculateAverage(const std::vector<double>& values) const {
    if (values.empty()) {
        return 0;
    }
    double sum = 0;
    for (double v : values) {
        sum += v;
    }
    return sum / values.size();
}

double PCIeDriver::calculatePercentile(const std::vector<double>& values, double percentile) const {
    if (values.empty()) {
        return 0;
    }
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    size_t index = static_cast<size_t>(percentile / 100.0 * (sorted.size() - 1));
    return sorted[index];
}

void PCIeDriver::delayMicroseconds(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

void PCIeDriver::notifyDeviceDetected(const DeviceInfo& info) {
    if (on_device_detected) {
        on_device_detected(info);
    }
}

void PCIeDriver::notifyDeviceRemoved(const DeviceInfo& info) {
    if (on_device_removed) {
        on_device_removed(info);
    }
}

void PCIeDriver::notifyDeviceChanged(const DeviceInfo& info) {
    if (on_device_changed) {
        on_device_changed(info);
    }
}

void PCIeDriver::recordError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
}

// ============================================
// NVME INFO (ADDITIONAL)
// ============================================

uint32_t PCIeDriver::getNVMeTemperature(const std::string& device_path) const {
    auto it = nvme_devices.find(device_path);
    if (it != nvme_devices.end()) {
        return it->second.temperature_celsius;
    }
    return 0;
}

uint8_t PCIeDriver::getNVMeHealth(const std::string& device_path) const {
    auto it = nvme_devices.find(device_path);
    if (it != nvme_devices.end()) {
        return 100 - it->second.percentage_used;
    }
    return 0;
}

std::string PCIeDriver::getNVMeSMART(const std::string& device_path) const {
    std::string output;
    std::string cmd = "nvme smart-log " + device_path + " 2>/dev/null";
    executeCommand(cmd, output);
    return output;
}
