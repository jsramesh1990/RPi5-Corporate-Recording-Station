#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <cstdint>
#include <string>

/**
 * @brief System-wide constants for Recording Station
 */
namespace Constants {

// ============================================
// VERSION INFORMATION
// ============================================
constexpr const char* VERSION = "2.0.0";
constexpr const char* BUILD_DATE = __DATE__;
constexpr const char* BUILD_TIME = __TIME__;

// ============================================
// SYSTEM PATHS
// ============================================
constexpr const char* DEFAULT_DATA_DIR = "/var/lib/recording-station";
constexpr const char* DEFAULT_RECORDINGS_DIR = "/var/lib/recording-station/recordings";
constexpr const char* DEFAULT_ARCHIVE_DIR = "/var/lib/recording-station/archive";
constexpr const char* DEFAULT_TEMP_DIR = "/var/lib/recording-station/temp";
constexpr const char* DEFAULT_METADATA_DIR = "/var/lib/recording-station/metadata";
constexpr const char* DEFAULT_LOG_DIR = "/var/log/recording-station";
constexpr const char* DEFAULT_CONFIG_DIR = "/etc/recording-station";

// ============================================
// RECORDING CONSTANTS
// ============================================
constexpr int DEFAULT_VIDEO_WIDTH = 1920;
constexpr int DEFAULT_VIDEO_HEIGHT = 1080;
constexpr int DEFAULT_VIDEO_FRAMERATE = 30;
constexpr int DEFAULT_VIDEO_BITRATE = 4000000;  // 4 Mbps
constexpr int DEFAULT_AUDIO_SAMPLE_RATE = 48000;
constexpr int DEFAULT_AUDIO_CHANNELS = 2;
constexpr int DEFAULT_AUDIO_BITRATE = 128000;   // 128 Kbps
constexpr int MAX_RECORDING_DURATION = 86400;   // 24 hours
constexpr int64_t MAX_FILE_SIZE = 4LL * 1024 * 1024 * 1024;  // 4 GB
constexpr int64_t MIN_FREE_SPACE = 10LL * 1024 * 1024 * 1024;  // 10 GB

// ============================================
// HARDWARE CONSTANTS
// ============================================
constexpr const char* DEFAULT_UART_DEVICE = "/dev/ttyAMA0";
constexpr int DEFAULT_UART_BAUD = 115200;
constexpr const char* DEFAULT_CAMERA_DEVICE = "/dev/video0";
constexpr const char* DEFAULT_AUDIO_DEVICE = "hw:0,0";

// GPIO Pins
constexpr int GPIO_SYSTEM_LED = 17;
constexpr int GPIO_RECORDING_LED = 27;
constexpr int GPIO_START_BUTTON = 22;
constexpr int GPIO_STOP_BUTTON = 23;
constexpr int GPIO_ERROR_LED = 24;
constexpr int GPIO_STATUS_LED = 25;

// ============================================
// NETWORK CONSTANTS
// ============================================
constexpr int DEFAULT_WEB_PORT = 8080;
constexpr int DEFAULT_WEBSOCKET_PORT = 8081;
constexpr const char* DEFAULT_HOST = "0.0.0.0";
constexpr int DEFAULT_CONNECTION_TIMEOUT = 30;  // seconds
constexpr int DEFAULT_RETRY_COUNT = 3;
constexpr int DEFAULT_RETRY_DELAY = 5;  // seconds

// ============================================
// MEMORY CONSTANTS
// ============================================
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t ZRAM_SIZE_MB = 1024;
constexpr int SWAPPINESS_DEFAULT = 80;
constexpr size_t MAX_MEMORY_USAGE_MB = 2048;
constexpr int MEMORY_WARNING_LEVEL = 80;  // percentage

// ============================================
// TIMING CONSTANTS
// ============================================
constexpr int STATUS_UPDATE_INTERVAL = 5;  // seconds
constexpr int LOG_ROTATION_INTERVAL = 86400;  // 24 hours
constexpr int HEALTH_CHECK_INTERVAL = 60;  // seconds
constexpr int AUTO_SAVE_INTERVAL = 300;  // 5 minutes

// ============================================
// CLOUD CONSTANTS
// ============================================
constexpr int CLOUD_UPLOAD_TIMEOUT = 300;  // 5 minutes
constexpr int CLOUD_RETRY_COUNT = 3;
constexpr int CLOUD_RETRY_DELAY = 10;  // seconds
constexpr int CLOUD_SYNC_INTERVAL = 3600;  // 1 hour

// ============================================
// LOGGING CONSTANTS
// ============================================
constexpr const char* DEFAULT_LOG_LEVEL = "INFO";
constexpr size_t MAX_LOG_FILE_SIZE = 10 * 1024 * 1024;  // 10 MB
constexpr int MAX_LOG_FILES = 5;

// ============================================
// ERROR CODES
// ============================================
enum ErrorCode {
    SUCCESS = 0,
    ERROR_GENERAL = -1,
    ERROR_CONFIG = -2,
    ERROR_HARDWARE = -3,
    ERROR_MEMORY = -4,
    ERROR_STORAGE = -5,
    ERROR_NETWORK = -6,
    ERROR_RECORDING = -7,
    ERROR_CLOUD = -8,
    ERROR_TIMEOUT = -9,
    ERROR_PERMISSION = -10,
    ERROR_NOT_FOUND = -11,
    ERROR_BUSY = -12,
    ERROR_INVALID = -13,
    ERROR_IO = -14,
};

// ============================================
// FILE FORMATS
// ============================================
constexpr const char* VIDEO_FORMAT_MP4 = "mp4";
constexpr const char* VIDEO_FORMAT_MKV = "mkv";
constexpr const char* VIDEO_FORMAT_AVI = "avi";
constexpr const char* AUDIO_FORMAT_WAV = "wav";
constexpr const char* AUDIO_FORMAT_MP3 = "mp3";
constexpr const char* AUDIO_FORMAT_FLAC = "flac";
constexpr const char* AUDIO_FORMAT_AAC = "aac";

constexpr const char* VIDEO_CODEC_H264 = "h264";
constexpr const char* VIDEO_CODEC_H265 = "h265";
constexpr const char* VIDEO_CODEC_MJPEG = "mjpeg";
constexpr const char* AUDIO_CODEC_AAC = "aac";
constexpr const char* AUDIO_CODEC_MP3 = "mp3";
constexpr const char* AUDIO_CODEC_OPUS = "opus";

// ============================================
// CLOUD PROVIDERS
// ============================================
constexpr const char* CLOUD_NEXTCLOUD = "nextcloud";
constexpr const char* CLOUD_ONEDRIVE = "onedrive";
constexpr const char* CLOUD_S3 = "s3";
constexpr const char* CLOUD_WEBDAV = "webdav";

// ============================================
// DEVICE TYPES
// ============================================
enum DeviceType {
    DEVICE_CAMERA_CSI,
    DEVICE_CAMERA_USB,
    DEVICE_CAMERA_HDMI,
    DEVICE_AUDIO_USB,
    DEVICE_AUDIO_I2S,
    DEVICE_AUDIO_HDMI,
    DEVICE_STORAGE_NVME,
    DEVICE_STORAGE_SATA,
    DEVICE_STORAGE_USB,
    DEVICE_STORAGE_SD
};

// ============================================
// SYSTEM STATES
// ============================================
enum SystemState {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PROCESSING,
    STATE_UPLOADING,
    STATE_ERROR,
    STATE_SHUTDOWN
};

} // namespace Constants

#endif // CONSTANTS_HPP
