# Changelog

All notable changes to Samsung AC HTTP Bridge will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2025-01-06

### Added
- **Reliable communication with retry mechanism**
  - Commands now wait for ACK packets from AC units
  - Automatic retry up to 3 times with 1 second timeout
  - Sequence numbers track command/ACK pairs
  - Only one active command at a time to prevent conflicts
  
- **Live web-based debug console** (`/debug`)
  - Real-time ESP32 serial output streaming
  - Connection status indicator (Connected/Disconnected/Error)
  - Free memory monitoring
  - Message buffer with last 100 messages
  - Auto-scroll and clear console functions
  - Updates every 500ms
  
- **Enhanced NASA protocol support**
  - Full documentation of all protocol messages from wiki
  - Proper handling of all message types
  - Removed "undocumented" message spam
  - Better error code handling

### Changed
- Improved command queue processing
- Commands are now confirmed by state changes
- Better memory management for debug messages
- IP address display fixed (was showing as number)
- "Free heap" renamed to "Free memory" in UI

### Fixed
- Debug console connection status now properly updates
- Fixed JavaScript errors in debug console
- Fixed message buffer overflow issues
- Fixed static HTML generation problems
- Removed excessive debug output for cleaner logs

### Technical Details
- Command queue implementation in `CommandQueue.h/cpp`
- Debug streaming via JSON polling (not WebSocket)
- Sequence numbers use 16-bit counter
- ACK timeout set to 1000ms
- Retry delay set to 1000ms
- Max retries set to 3

## [1.0.0] - 2024-12-20

### Initial Release
- Basic HTTP REST API for Samsung AC control
- Device auto-discovery
- Real-time status monitoring
- UDP broadcast to Loxone
- OTA firmware updates
- M5Stack Atom Lite support