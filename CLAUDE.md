# CLAUDE.md

This file provides guidance to Claude Code when working with this repository.

## Project Overview

Samsung AC HTTP Bridge is an M5Stack Atom Lite-based HTTP server that provides REST API access to Samsung air conditioners via RS485 communication using the NASA protocol. The project is optimized for the limited 520KB SRAM of the M5Stack Atom Lite.

## Development Commands

### Build and Upload
```bash
# Compile for M5Stack Atom Lite
pio run -e m5stack-atom

# Upload via USB
pio run -e m5stack-atom --target upload

# OTA update (requires device on network)
pio run -e m5stack-atom-ota --target upload

# Monitor serial output
pio device monitor
```

### Configuration
- Two environments: `m5stack-atom` (USB) and `m5stack-atom-ota` (OTA)
- Hardware: M5Stack Atom Lite with ESP32 PICO-D4 (520KB SRAM)
- Serial monitoring at 115200 baud
- Upload speed: 1500000 baud
- OTA hostname: `samsung-ac-bridge.local`, password: `samsung123`
- Uses M5Atom library (not M5Unified for memory optimization)

## Architecture

The codebase follows a layered architecture optimized for minimal memory usage:

### Core Components

1. **Protocol Layer** (`NasaProtocol.h/.cpp`)
   - Implements Samsung NASA protocol for RS485 communication
   - Packet structure: `[0x32][Size][SA][DA][Command][Messages][CRC16][0x34]`
   - Handles CRC16 validation and packet parsing
   - Supports message types: Enum, Variable, LongVariable, Structure
   - Preset support: None, Sleep, Quiet, Fast, Longreach, Eco, Windfree

2. **Bridge Layer** (`SamsungACBridge.h/.cpp`)
   - Manages device states and communication
   - Implements Bridge pattern between protocol and HTTP API
   - Handles device discovery and state synchronization
   - Performance monitoring and heap tracking
   - Uses Observer pattern with `MessageTarget` interface

3. **API Layer** (`main.cpp`)
   - Single HTTP server on port 80 for all endpoints
   - WiFi management and mDNS hostname resolution
   - OTA update functionality
   - Memory-optimized JSON responses
   - Request timing and heap usage monitoring

### Communication Flow
```
HTTP Request → SamsungACBridge → NasaProtocol → RS485 → Samsung AC
                      ↓
Device State ← MessageTarget ← Packet Processing ← RS485 Response
```

### Hardware Configuration
- M5Stack Atom Lite with RGB LED disabled for memory saving
- Serial2 (GPIO 22/19) at 9600 baud, even parity for RS485
- RS485 to TTL converter required for AC communication
- WiFi for HTTP API access

## Memory Optimizations

Critical optimizations for 520KB SRAM limitation:

- **Small JSON buffers**: 400-512 bytes instead of 1024+
- **LED display disabled**: `M5.begin(true, false, false)`
- **Debug logging disabled**: `DEBUG_ENABLED = 0`, `CORE_DEBUG_LEVEL = 0`
- **No telnet server**: Removed to save ~8KB
- **String pre-allocation**: Using `reserve()` to reduce fragmentation
- **Heap monitoring**: Automatic garbage collection when memory low
- **Simplified protocol**: Removed unused product info features

## Key Design Patterns

- **Bridge Pattern**: `SamsungACBridge` abstracts protocol from API
- **Observer Pattern**: `MessageTarget` for protocol message handling
- **State Pattern**: `DeviceState` struct maintains device status
- **Factory Pattern**: `Packet::create()` for protocol packet creation

## REST API Endpoints

### System and Diagnostics
- `GET /` - System info with performance metrics
- `GET /stats` - Performance and memory statistics
- `GET /wifi` - WiFi status and signal strength

### Device Management
- `GET /devices` - List all discovered devices
- `GET /device?address=XX.XX.XX` - Get device state
- `GET /device/sensors?address=XX.XX.XX` - Get sensor readings
- `POST /device/control` - Control device (JSON payload)

### Firmware Update
- `GET /update` - Web OTA interface
- `POST /update` - Upload firmware binary

## Control Parameters

### Supported Parameters
- **Power**: `true`/`false`
- **Mode**: "auto", "cool", "dry", "fan", "heat" or 0-4
- **Target Temperature**: 16.0-30.0°C
- **Fan Mode**: "auto", "low", "mid", "high", "turbo" or 0-4
- **Swing**: `true`/`false` for vertical/horizontal
- **Preset**: "none", "sleep", "quiet", "fast", "longreach", "eco", "windfree" or 0-9

### API Accepts Both Formats
- String names: `{"mode": "cool", "fan_mode": "mid", "preset": "quiet"}`
- Integer values: `{"mode": 1, "fan_mode": 2, "preset": 2}`

## Configuration Files

### platformio.ini
- Platform: espressif32@6.3.2
- Board: m5stack-atom
- Dependencies: ArduinoJson@6.21.3, M5Atom, FastLED
- Debug level: `CORE_DEBUG_LEVEL=0` (disabled)
- Two build environments for USB and OTA deployment

### config.h
- M5Atom library initialization
- Debug macros (disabled in production)
- Centralized configuration constants

### main.cpp
- M5.begin() with LED display disabled
- WiFi credentials: hardcoded (change before deployment)
- mDNS hostname: `samsung-ac-bridge.local`
- Single HTTP server handling all endpoints

## Protocol Configuration

Samsung AC devices use specific settings:
- **Baud rate**: 9600 (not 2400)
- **Parity**: Even (SERIAL_8E1)
- **Communication pins**: GPIO 22 (RX), GPIO 19 (TX) for M5Stack Atom

## Device Addressing

Samsung AC devices use specific address ranges:
- Indoor units: 0x20.XX.XX
- Outdoor units: 0x10.XX.XX  
- WiFi Kit: 0x62.XX.XX
- Wired Remote: 0x50.XX.XX

## Performance Monitoring

Built-in monitoring capabilities:
- Request timing in all responses
- Heap usage tracking
- Memory fragmentation monitoring
- Loop performance metrics
- Automatic garbage collection

## Security Considerations

Current implementation for development use:
- Hardcoded WiFi credentials
- No HTTP authentication
- Default OTA password
- CORS enabled for all origins

For production deployment, consider:
- WiFi configuration via captive portal
- HTTP basic authentication
- Custom OTA passwords
- CORS origin restrictions

## Development Notes

- Based on ESPHome Samsung HVAC integration
- Optimized specifically for M5Stack Atom Lite hardware
- Memory constraints require careful buffer management
- Protocol implementation handles Samsung-specific timing
- OTA updates preserve device configuration
- No serial debugging available without USB connection