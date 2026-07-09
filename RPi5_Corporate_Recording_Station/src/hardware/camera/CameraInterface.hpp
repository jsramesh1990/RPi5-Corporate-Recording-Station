#include "CameraInterface.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>

// V4L2 headers
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// JPEG encoding (simplified - in production use libjpeg)
// For now, we'll use a simple placeholder

#ifdef __linux__
#include <sys/syscall.h>
#include <pthread.h>
#endif

CameraInterface::CameraInterface() 
    : initialized(false), 
      capture_running(false), 
      capture_paused(false),
      v4l2_fd(-1),
      v4l2_mmap(nullptr),
      v4l2_mmap_size(0) {
    
    // Initialize default configuration
    config.device = "/dev/video0";
    config.type = CameraType::CSI;
    config.width = 1920;
    config.height = 1080;
    config.framerate = 30;
    config.format = PixelFormat::YUYV;
    config.bitrate = 4000000;
    config.buffer_count = 4;
    config.auto_exposure = true;
    config.auto_white_balance = true;
    config.auto_focus = true;
    config.brightness = 128;
    config.contrast = 128;
    config.saturation = 128;
    config.sharpness = 128;
    
    // Set thread priority
    set_thread_priority(-10);
}

CameraInterface::~CameraInterface() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool CameraInterface::initialize(const CameraConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        shutdown_v4l2();
        initialized = false;
    }
    
    config = cfg;
    
    // Determine camera type if not specified
    if (config.type == CameraType::CSI && config.device.find("video") != std::string::npos) {
        config.type = CameraType::CSI;
    }
    
    bool success = false;
    
    switch (config.type) {
        case CameraType::CSI:
            success = init_csi(config);
            break;
        case CameraType::USB:
            success = init_v4l2(config);
            break;
        case CameraType::HDMI:
            success = init_hdmi(config);
            break;
        default:
            success = init_v4l2(config);
            break;
    }
    
    if (!success) {
        std::cerr << "Failed to initialize camera: " << config.device << std::endl;
        return false;
    }
    
    initialized = true;
    state = CameraState::IDLE;
    reset_stats();
    
    std::cout << "Camera initialized: " << config.device 
              << " (" << config.width << "x" << config.height 
              << " @ " << config.framerate << "fps)" << std::endl;
    
    return true;
}

bool CameraInterface::initialize(const std::string& device, int width, int height, int framerate) {
    CameraConfig cfg;
    cfg.device = device;
    cfg.width = width;
    cfg.height = height;
    cfg.framerate = framerate;
    return initialize(cfg);
}

bool CameraInterface::initialize_csi(const CameraConfig& cfg) {
    // CSI cameras use V4L2 as well, but with specific settings
    return init_v4l2(cfg);
}

bool CameraInterface::initialize_usb(const CameraConfig& cfg) {
    return init_v4l2(cfg);
}

bool CameraInterface::initialize_hdmi(const CameraConfig& cfg) {
    // HDMI capture cards also use V4L2
    return init_v4l2(cfg);
}

void CameraInterface::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (capture_running) {
        stop_capture();
    }
    
    if (streaming) {
        stop_streaming();
    }
    
    shutdown_v4l2();
    shutdown_csi();
    shutdown_hdmi();
    
    initialized = false;
    state = CameraState::IDLE;
    
    std::cout << "Camera shutdown" << std::endl;
}

// ============================================
// V4L2 IMPLEMENTATION
// ============================================

bool CameraInterface::init_v4l2(const CameraConfig& cfg) {
    // Open device
    v4l2_fd = open(cfg.device.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (v4l2_fd < 0) {
        std::cerr << "Failed to open device: " << cfg.device 
                  << " (" << strerror(errno) << ")" << std::endl;
        return false;
    }
    
    // Configure device
    if (!configure_v4l2(cfg)) {
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }
    
    // Setup memory mapping
    if (!setup_v4l2_mmap()) {
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }
    
    return true;
}

void CameraInterface::shutdown_v4l2() {
    if (v4l2_fd >= 0) {
        // Stop capture if running
        stop_v4l2_capture();
        
        // Cleanup memory mapping
        cleanup_v4l2_mmap();
        
        close(v4l2_fd);
        v4l2_fd = -1;
    }
}

bool CameraInterface::configure_v4l2(const CameraConfig& cfg) {
    struct v4l2_capability cap;
    if (xioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        print_v4l2_error(errno, "Failed to query capabilities");
        return false;
    }
    
    // Check capabilities
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::cerr << "Device does not support video capture" << std::endl;
        return false;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        std::cerr << "Device does not support streaming" << std::endl;
        return false;
    }
    
    // Set format
    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = cfg.width;
    fmt.fmt.pix.height = cfg.height;
    fmt.fmt.pix.pixelformat = format_to_fourcc(cfg.format);
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (xioctl(v4l2_fd, VIDIOC_S_FMT, &fmt) < 0) {
        print_v4l2_error(errno, "Failed to set format");
        return false;
    }
    
    // Verify format
    if (fmt.fmt.pix.width != (unsigned int)cfg.width || 
        fmt.fmt.pix.height != (unsigned int)cfg.height) {
        std::cout << "Resolution adjusted to " << fmt.fmt.pix.width << "x" 
                  << fmt.fmt.pix.height << std::endl;
        config.width = fmt.fmt.pix.width;
        config.height = fmt.fmt.pix.height;
    }
    
    // Set framerate
    struct v4l2_streamparm parm = {};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = cfg.framerate;
    
    if (xioctl(v4l2_fd, VIDIOC_S_PARM, &parm) < 0) {
        print_v4l2_error(errno, "Failed to set framerate");
        // Non-fatal, continue
    }
    
    // Set controls
    struct v4l2_control ctrl;
    
    // Brightness
    ctrl.id = V4L2_CID_BRIGHTNESS;
    ctrl.value = cfg.brightness;
    xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    
    // Contrast
    ctrl.id = V4L2_CID_CONTRAST;
    ctrl.value = cfg.contrast;
    xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    
    // Saturation
    ctrl.id = V4L2_CID_SATURATION;
    ctrl.value = cfg.saturation;
    xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    
    // Sharpness
    ctrl.id = V4L2_CID_SHARPNESS;
    ctrl.value = cfg.sharpness;
    xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    
    // Auto exposure
    if (cfg.auto_exposure) {
        ctrl.id = V4L2_CID_EXPOSURE_AUTO;
        ctrl.value = V4L2_EXPOSURE_AUTO;
        xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    }
    
    // Auto white balance
    if (cfg.auto_white_balance) {
        ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
        ctrl.value = 1;
        xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    }
    
    // Auto focus
    if (cfg.auto_focus) {
        ctrl.id = V4L2_CID_FOCUS_AUTO;
        ctrl.value = 1;
        xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    }
    
    return true;
}

bool CameraInterface::setup_v4l2_mmap() {
    struct v4l2_requestbuffers req = {};
    req.count = config.buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(v4l2_fd, VIDIOC_REQBUFS, &req) < 0) {
        print_v4l2_error(errno, "Failed to request buffers");
        return false;
    }
    
    if (req.count < 2) {
        std::cerr << "Insufficient buffer memory" << std::endl;
        return false;
    }
    
    v4l2_buffers.resize(req.count);
    v4l2_buffer_sizes.resize(req.count);
    
    for (unsigned int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (xioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            print_v4l2_error(errno, "Failed to query buffer");
            return false;
        }
        
        v4l2_buffers[i] = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, v4l2_fd, buf.m.offset);
        if (v4l2_buffers[i] == MAP_FAILED) {
            print_v4l2_error(errno, "Failed to mmap buffer");
            return false;
        }
        v4l2_buffer_sizes[i] = buf.length;
    }
    
    // Queue all buffers
    for (unsigned int i = 0; i < req.count; i++) {
        if (!queue_v4l2_buffer(i)) {
            return false;
        }
    }
    
    return true;
}

void CameraInterface::cleanup_v4l2_mmap() {
    for (size_t i = 0; i < v4l2_buffers.size(); i++) {
        if (v4l2_buffers[i] != nullptr) {
            munmap(v4l2_buffers[i], v4l2_buffer_sizes[i]);
            v4l2_buffers[i] = nullptr;
        }
    }
    v4l2_buffers.clear();
    v4l2_buffer_sizes.clear();
}

bool CameraInterface::start_v4l2_capture() {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(v4l2_fd, VIDIOC_STREAMON, &type) < 0) {
        print_v4l2_error(errno, "Failed to start streaming");
        return false;
    }
    return true;
}

void CameraInterface::stop_v4l2_capture() {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(v4l2_fd, VIDIOC_STREAMOFF, &type);
}

bool CameraInterface::queue_v4l2_buffer(int index) {
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    
    if (xioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0) {
        print_v4l2_error(errno, "Failed to queue buffer");
        return false;
    }
    return true;
}

bool CameraInterface::dequeue_v4l2_buffer(int* index) {
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno != EAGAIN) {
            print_v4l2_error(errno, "Failed to dequeue buffer");
        }
        return false;
    }
    
    *index = buf.index;
    return true;
}

FrameInfo CameraInterface::read_v4l2_frame() {
    FrameInfo frame;
    
    int index;
    if (!dequeue_v4l2_buffer(&index)) {
        return frame;
    }
    
    // Copy frame data
    if (index >= 0 && index < (int)v4l2_buffers.size()) {
        uint8_t* data = static_cast<uint8_t*>(v4l2_buffers[index]);
        size_t size = v4l2_buffer_sizes[index];
        frame.data.assign(data, data + size);
        frame.width = config.width;
        frame.height = config.height;
        frame.format = config.format;
        frame.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        frame.frame_number = stats.frames_captured + 1;
        frame.is_keyframe = true;
    }
    
    // Requeue buffer
    queue_v4l2_buffer(index);
    
    return frame;
}

int CameraInterface::xioctl(int fd, int request, void* arg) {
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

void CameraInterface::print_v4l2_error(int err, const std::string& context) {
    std::cerr << context << ": " << strerror(err) << " (errno=" << err << ")" << std::endl;
}

// ============================================
// CSI SPECIFIC
// ============================================

bool CameraInterface::init_csi(const CameraConfig& cfg) {
    // CSI cameras use V4L2 with specific overlays
    // The actual initialization is done through V4L2
    return init_v4l2(cfg);
}

void CameraInterface::shutdown_csi() {
    // Cleanup is handled by shutdown_v4l2
}

bool CameraInterface::configure_csi(const CameraConfig& cfg) {
    // Specific CSI configuration
    // For Raspberry Pi, this would use libcamera or MMAL
    // For now, we use V4L2
    return configure_v4l2(cfg);
}

// ============================================
// HDMI SPECIFIC
// ============================================

bool CameraInterface::init_hdmi(const CameraConfig& cfg) {
    // HDMI capture cards also use V4L2
    return init_v4l2(cfg);
}

void CameraInterface::shutdown_hdmi() {
    // Cleanup is handled by shutdown_v4l2
}

bool CameraInterface::configure_hdmi(const CameraConfig& cfg) {
    // HDMI capture specific settings
    return configure_v4l2(cfg);
}

// ============================================
// CAPTURE
// ============================================

bool CameraInterface::start_capture() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        notify_error("Not initialized");
        return false;
    }
    
    if (capture_running) {
        return true;
    }
    
    if (!start_v4l2_capture()) {
        return false;
    }
    
    capture_running = true;
    capture_paused = false;
    capture_thread = std::thread(&CameraInterface::capture_loop, this);
    
    set_state(CameraState::CAPTURING);
    reset_stats();
    
    std::cout << "Camera capture started" << std::endl;
    return true;
}

void CameraInterface::stop_capture() {
    if (!capture_running) {
        return;
    }
    
    capture_running = false;
    capture_paused = false;
    
    if (capture_thread.joinable()) {
        capture_thread.join();
    }
    
    stop_v4l2_capture();
    
    set_state(CameraState::IDLE);
    std::cout << "Camera capture stopped" << std::endl;
}

void CameraInterface::pause_capture() {
    if (capture_running && !capture_paused) {
        capture_paused = true;
        set_state(CameraState::PAUSED);
        std::cout << "Camera capture paused" << std::endl;
    }
}

void CameraInterface::resume_capture() {
    if (capture_running && capture_paused) {
        capture_paused = false;
        set_state(CameraState::CAPTURING);
        std::cout << "Camera capture resumed" << std::endl;
    }
}

FrameInfo CameraInterface::capture_frame() {
    return capture_frame(5000); // 5 second timeout
}

FrameInfo CameraInterface::capture_frame(int timeout_ms) {
    if (!initialized || !capture_running) {
        return FrameInfo();
    }
    
    auto start = std::chrono::steady_clock::now();
    while (capture_running && !capture_paused) {
        FrameInfo frame = read_v4l2_frame();
        if (!frame.data.empty()) {
            update_stats(frame);
            return frame;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return FrameInfo();
}

FrameInfo CameraInterface::get_current_frame() const {
    std::lock_guard<std::mutex> lock(frame_mutex);
    return current_frame;
}

void CameraInterface::capture_loop() {
    // Set thread priority
    set_thread_priority(-10);
    set_thread_affinity(2);
    
    while (capture_running) {
        if (capture_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        FrameInfo frame = read_v4l2_frame();
        if (!frame.data.empty()) {
            // Update current frame
            {
                std::lock_guard<std::mutex> lock(frame_mutex);
                current_frame = frame;
                has_frame = true;
            }
            
            // Process frame
            process_frame(frame);
        }
    }
}

void CameraInterface::process_frame(const FrameInfo& frame) {
    update_stats(frame);
    
    if (on_frame) {
        on_frame(frame);
    }
}

// ============================================
// STREAMING
// ============================================

bool CameraInterface::start_streaming() {
    if (!start_capture()) {
        return false;
    }
    
    streaming = true;
    std::cout << "Camera streaming started" << std::endl;
    return true;
}

void CameraInterface::stop_streaming() {
    stop_capture();
    streaming = false;
    std::cout << "Camera streaming stopped" << std::endl;
}

void CameraInterface::set_on_frame(FrameCallback callback) {
    on_frame = callback;
}

// ============================================
// CONFIGURATION
// ============================================

bool CameraInterface::set_resolution(int width, int height) {
    if (initialized) {
        return false;
    }
    config.width = width;
    config.height = height;
    return true;
}

bool CameraInterface::set_framerate(int framerate) {
    if (initialized) {
        return false;
    }
    config.framerate = framerate;
    return true;
}

bool CameraInterface::set_bitrate(int bitrate) {
    config.bitrate = bitrate;
    return true;
}

bool CameraInterface::set_exposure(int exposure_us) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ctrl.value = exposure_us;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set exposure");
        return false;
    }
    return true;
}

bool CameraInterface::set_gain(int gain_db) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_GAIN;
    ctrl.value = gain_db;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set gain");
        return false;
    }
    return true;
}

bool CameraInterface::set_brightness(int brightness) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_BRIGHTNESS;
    ctrl.value = brightness;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set brightness");
        return false;
    }
    return true;
}

bool CameraInterface::set_contrast(int contrast) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_CONTRAST;
    ctrl.value = contrast;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set contrast");
        return false;
    }
    return true;
}

bool CameraInterface::set_saturation(int saturation) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_SATURATION;
    ctrl.value = saturation;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set saturation");
        return false;
    }
    return true;
}

bool CameraInterface::set_sharpness(int sharpness) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_SHARPNESS;
    ctrl.value = sharpness;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set sharpness");
        return false;
    }
    return true;
}

bool CameraInterface::set_rotation(int degrees) {
    config.rotation = degrees;
    return true;
}

bool CameraInterface::set_flip(bool horizontal, bool vertical) {
    config.flip_horizontal = horizontal;
    config.flip_vertical = vertical;
    return true;
}

bool CameraInterface::set_auto_exposure(bool enable) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_EXPOSURE_AUTO;
    ctrl.value = enable ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set auto exposure");
        return false;
    }
    return true;
}

bool CameraInterface::set_auto_white_balance(bool enable) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
    ctrl.value = enable ? 1 : 0;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set auto white balance");
        return false;
    }
    return true;
}

bool CameraInterface::set_auto_focus(bool enable) {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_FOCUS_AUTO;
    ctrl.value = enable ? 1 : 0;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to set auto focus");
        return false;
    }
    return true;
}

bool CameraInterface::trigger_autofocus() {
    if (!initialized || v4l2_fd < 0) {
        return false;
    }
    
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_FOCUS_AUTO;
    ctrl.value = 1;
    
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        print_v4l2_error(errno, "Failed to trigger autofocus");
        return false;
    }
    
    // Wait a bit for focus to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Turn off auto focus (if not in auto mode)
    if (!config.auto_focus) {
        ctrl.value = 0;
        xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl);
    }
    
    return true;
}

// ============================================
// STATISTICS
// ============================================

CameraInterface::CameraStats CameraInterface::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    return stats;
}

void CameraInterface::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats = CameraStats{};
    stats.start_time = std::chrono::system_clock::now();
    stats.last_frame_time = stats.start_time;
}

void CameraInterface::update_stats(const FrameInfo& frame) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    stats.frames_captured++;
    stats.total_bytes += frame.data.size();
    stats.last_frame_time = std::chrono::system_clock::now();
    
    // Calculate FPS
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        stats.last_frame_time - stats.start_time).count();
    if (elapsed > 0) {
        stats.fps_actual = static_cast<double>(stats.frames_captured) / elapsed;
    }
    
    // Calculate bitrate
    if (elapsed > 0) {
        stats.bitrate_actual = (stats.total_bytes * 8.0) / elapsed;
    }
    
    // Calculate frame time
    if (stats.frames_captured > 1) {
        stats.frame_time_ms = 1000.0 / stats.fps_actual;
    }
    
    stats.current_resolution = std::to_string(frame.width) + "x" + 
                               std::to_string(frame.height);
}

void CameraInterface::notify_stats() {
    if (on_stats) {
        auto stats_copy = get_stats();
        on_stats(stats_copy);
    }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void CameraInterface::set_state(CameraState new_state) {
    if (state != new_state) {
        state = new_state;
        if (on_state_change) {
            on_state_change(state);
        }
    }
}

void CameraInterface::notify_error(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
}

std::string CameraInterface::get_state_name() const {
    switch (state) {
        case CameraState::IDLE: return "IDLE";
        case CameraState::CAPTURING: return "CAPTURING";
        case CameraState::STREAMING: return "STREAMING";
        case CameraState::PAUSED: return "PAUSED";
        case CameraState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ============================================
// CALLBACKS
// ============================================

void CameraInterface::set_on_error(ErrorCallback callback) {
    on_error = callback;
}

void CameraInterface::set_on_state_change(StateCallback callback) {
    on_state_change = callback;
}

void CameraInterface::set_on_stats(StatsCallback callback) {
    on_stats = callback;
}

// ============================================
// UTILITY
// ============================================

bool CameraInterface::test_device() {
    std::cout << "Testing camera device..." << std::endl;
    
    if (!start_capture()) {
        return false;
    }
    
    // Capture a few frames
    for (int i = 0; i < 5; i++) {
        auto frame = capture_frame(1000);
        if (frame.data.empty()) {
            std::cout << "Failed to capture frame " << i << std::endl;
        } else {
            std::cout << "Frame " << i << ": " << frame.data.size() 
                      << " bytes" << std::endl;
        }
    }
    
    stop_capture();
    
    std::cout << "Camera test complete" << std::endl;
    return true;
}

std::string CameraInterface::get_format_name(PixelFormat format) const {
    switch (format) {
        case PixelFormat::YUYV: return "YUYV";
        case PixelFormat::YUV420: return "YUV420";
        case PixelFormat::RGB24: return "RGB24";
        case PixelFormat::RGB32: return "RGB32";
        case PixelFormat::MJPEG: return "MJPEG";
        case PixelFormat::H264: return "H264";
        case PixelFormat::H265: return "H265";
        case PixelFormat::GRAY8: return "GRAY8";
        case PixelFormat::GRAY16: return "GRAY16";
        case PixelFormat::RAW8: return "RAW8";
        case PixelFormat::RAW10: return "RAW10";
        case PixelFormat::RAW12: return "RAW12";
        default: return "UNKNOWN";
    }
}

std::string CameraInterface::get_type_name(CameraType type) const {
    switch (type) {
        case CameraType::CSI: return "CSI";
        case CameraType::USB: return "USB";
        case CameraType::HDMI: return "HDMI";
        case CameraType::NETWORK: return "NETWORK";
        default: return "UNKNOWN";
    }
}

uint32_t CameraInterface::format_to_fourcc(PixelFormat format) const {
    switch (format) {
        case PixelFormat::YUYV: return V4L2_PIX_FMT_YUYV;
        case PixelFormat::YUV420: return V4L2_PIX_FMT_YUV420;
        case PixelFormat::RGB24: return V4L2_PIX_FMT_RGB24;
        case PixelFormat::RGB32: return V4L2_PIX_FMT_RGB32;
        case PixelFormat::MJPEG: return V4L2_PIX_FMT_MJPEG;
        case PixelFormat::H264: return V4L2_PIX_FMT_H264;
        case PixelFormat::H265: return V4L2_PIX_FMT_HEVC;
        case PixelFormat::GRAY8: return V4L2_PIX_FMT_GREY;
        case PixelFormat::RAW8: return V4L2_PIX_FMT_SBGGR8;
        case PixelFormat::RAW10: return V4L2_PIX_FMT_SBGGR10;
        default: return V4L2_PIX_FMT_YUYV;
    }
}

PixelFormat CameraInterface::fourcc_to_format(uint32_t fourcc) const {
    switch (fourcc) {
        case V4L2_PIX_FMT_YUYV: return PixelFormat::YUYV;
        case V4L2_PIX_FMT_YUV420: return PixelFormat::YUV420;
        case V4L2_PIX_FMT_RGB24: return PixelFormat::RGB24;
        case V4L2_PIX_FMT_RGB32: return PixelFormat::RGB32;
        case V4L2_PIX_FMT_MJPEG: return PixelFormat::MJPEG;
        case V4L2_PIX_FMT_H264: return PixelFormat::H264;
        case V4L2_PIX_FMT_HEVC: return PixelFormat::H265;
        case V4L2_PIX_FMT_GREY: return PixelFormat::GRAY8;
        case V4L2_PIX_FMT_SBGGR8: return PixelFormat::RAW8;
        case V4L2_PIX_FMT_SBGGR10: return PixelFormat::RAW10;
        default: return PixelFormat::YUYV;
    }
}

bool CameraInterface::save_frame(const FrameInfo& frame, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(frame.data.data()), frame.data.size());
    file.close();
    return true;
}

std::vector<uint8_t> CameraInterface::encode_jpeg(const FrameInfo& frame, int quality) {
    // This is a simplified placeholder
    // In production, use libjpeg-turbo or similar
    // For now, return the raw data
    return frame.data;
}

// ============================================
// DEVICE MANAGEMENT
// ============================================

std::vector<CameraInterface::DeviceInfo> CameraInterface::list_devices() const {
    std::vector<DeviceInfo> devices;
    
    // Scan /dev/video* for devices
    for (int i = 0; i < 10; i++) {
        std::string path = "/dev/video" + std::to_string(i);
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            DeviceInfo dev;
            dev.device_path = path;
            dev.name = "Camera " + std::to_string(i);
            dev.type = CameraType::USB;
            
            // Try to open device to query capabilities
            int fd = open(path.c_str(), O_RDWR);
            if (fd >= 0) {
                struct v4l2_capability cap;
                if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                    dev.name = reinterpret_cast<const char*>(cap.card);
                    
                    // Determine type
                    if (std::string(reinterpret_cast<const char*>(cap.driver)).find("bcm2835") != std::string::npos) {
                        dev.type = CameraType::CSI;
                    }
                }
                
                // Query supported formats
                struct v4l2_fmtdesc fmt;
                fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                fmt.index = 0;
                while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
                    dev.supported_formats.push_back(fourcc_to_format(fmt.pixelformat));
                    fmt.index++;
                }
                
                close(fd);
            }
            
            devices.push_back(dev);
        }
    }
    
    return devices;
}

std::string CameraInterface::get_default_device() const {
    return "/dev/video0";
}

bool CameraInterface::set_device(const std::string& device) {
    if (initialized) {
        return false;
    }
    config.device = device;
    return true;
}

// ============================================
// THREAD UTILITIES
// ============================================

void CameraInterface::set_thread_priority(int priority) {
    #ifdef __linux__
    struct sched_param param;
    param.sched_priority = 0;
    sched_setscheduler(0, SCHED_OTHER, &param);
    
    if (priority != 0) {
        nice(priority);
    }
    #endif
}

void CameraInterface::set_thread_affinity(int cpu) {
    #ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    #endif
}
