# Changelog

All notable changes to this project will be documented in this file.

## [2.0.0] - 2026-07-08

### Added
- Full hardware abstraction layer (GPIO, UART, I2C, SPI, PWM)
- WebSocket support for real-time dashboard updates
- Multiple cloud providers (Nextcloud, OneDrive, AWS S3)
- Hardware H.264 encoding with VideoCore VII
- ZRAM compression support (4.5x memory efficiency)
- UART command interface with 10+ commands
- Systemd service for auto-start
- Docker containerization
- Comprehensive test suite (unit, integration, performance)
- Hardware calibration tool
- 3D printable enclosure design
- Bill of Materials (BOM) spreadsheet

### Changed
- Restructured repository for better organization
- Moved hardware drivers to separate subdirectories
- Updated CMake build system for cross-compilation
- Enhanced logging with spdlog
- Improved memory management with buddy allocator

### Fixed
- UART console stability issues
- Memory leak in video encoder
- GPIO interrupt handling
- Cloud upload retry logic
- Web dashboard refresh rate

## [1.0.0] - 2026-06-15

### Added
- Initial release
- HDMI capture support
- Audio recording
- Basic storage monitoring
- Simple web dashboard
- UART console
- Memory management
