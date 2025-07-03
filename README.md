# Samsung AC HTTP Bridge

A standalone HTTP bridge for Samsung air conditioning units using the NASA protocol. This project allows you to control and monitor Samsung AC units locally through a simple REST API without requiring ESPHome or Home Assistant. Best approach for Loxone, NodeRED etc.

Tested with Samsung Windfree AJ050TXJ2KG multi-split unit.

## Features

- **Simple HTTP REST API** for device control and monitoring
- **Samsung NASA protocol support** for direct communication with AC units
- **Device auto-discovery** - automatically detects connected Samsung AC devices
- **Real-time status monitoring** including temperatures, power consumption, error codes
- **Preset support** - Quiet, Windfree, Fast, Sleep, Eco modes
- **OTA firmware updates** via web interface or PlatformIO
- **WiFi diagnostics** and performance monitoring
- **CORS support** for web applications
- **Optimized for M5Stack Atom Lite** with minimal memory footprint

## Hardware Requirements

- **M5Stack Atom Lite** (ESP32 with 520KB SRAM)
- **RS485 to TTL converter** for communication with Samsung indoor units
- Proper wiring to Samsung AC communication bus (F1/F2) and power from main board (12V)

## Wiring

Connect your M5Stack Atom Lite to the Samsung AC communication bus:

```
RS485 A -> Samsung AC Bus F1
RS485 B -> Samsung AC Bus F2
Common Ground and +12V power between M5Stack and AC unit
```

You can find more here: https://github.com/omerfaruk-aran/esphome_samsung_hvac_bus/wiki/Hardware-Installation

## Configuration

1. Update WiFi credentials in `src/main.cpp`:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```

2. UART configuration is optimized for Samsung AC (9600 baud, even parity):
   ```cpp
   bridge.begin(22, 19, 9600); // RX, TX, baud rate
   ```

## Building and Uploading

This project uses PlatformIO with M5Stack Atom Lite configuration.

### Initial Upload (USB)
```bash
cd samsung_ac_http_bridge
pio run -e m5stack-atom --target upload
```

### OTA Updates
After initial upload, update remotely:

#### Web Interface OTA
1. Go to `http://samsung-ac-bridge.local/update`
2. Build firmware: `pio run -e m5stack-atom`
3. Upload `.pio/build/m5stack-atom/firmware.bin`

#### PlatformIO OTA
```bash
pio run -e m5stack-atom-ota --target upload
```

**Note:** Default OTA password is `samsung123`

## API Documentation

### System Information

#### `GET /`
Returns system information and performance stats.

**Response:**
```json
{
  "name": "Samsung AC HTTP Bridge",
  "version": "1.0.0",
  "uptime": 123456,
  "free_heap": 180000,
  "min_free_heap": 170000,
  "heap_fragmentation": 5.2,
  "loop_time_ms": 2,
  "max_loop_time_ms": 15
}
```

#### `GET /stats`
Performance and memory statistics.

**Response:**
```json
{
  "bridge_requests": 1234,
  "avg_processing_time_ms": 2.5,
  "free_heap": 180000,
  "min_free_heap": 170000,
  "uptime_seconds": 3600
}
```

#### `GET /wifi`
WiFi connection status and signal strength.

**Response:**
```json
{
  "connected": true,
  "ssid": "MyWiFi",
  "ip_address": "192.168.1.100",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "rssi": -55,
  "signal_strength_percent": 90,
  "signal_quality": "Good",
  "channel": 6,
  "uptime_seconds": 3600
}
```

### Device Discovery

#### `GET /devices`
List all discovered Samsung AC devices.

**Response:**
```json
{
  "devices": [
    {
      "address": "10.00.00",
      "type": "Outdoor",
      "online": true
    },
    {
      "address": "20.00.00",
      "type": "Indoor",
      "online": true
    },
    {
      "address": "20.00.01",
      "type": "Indoor",
      "online": true
    }
  ]
}
```

### Device Status

#### `GET /device?address=XX.XX.XX`
Get complete device status.

**Parameters:**
- `address` - Device address (e.g., "20.00.00")

**Response:**
```json
{
  "address": "20.00.00",
  "online": true,
  "power": true,
  "mode": 1,
  "target_temperature": 22.0,
  "room_temperature": 24.5,
  "fan_mode": 2,
  "swing_vertical": false,
  "swing_horizontal": false,
  "preset": "none",
  "request_time_ms": 3,
  "heap_used": 512
}
```

#### `GET /device/sensors?address=XX.XX.XX`
Get all sensor readings for a device.

**Parameters:**
- `address` - Device address

**Response:**
```json
{
  "address": "10.00.00",
  "room_temperature": 0.0,
  "target_temperature": 0.0,
  "outdoor_temperature": 36.1,
  "eva_in_temperature": 0.0,
  "eva_out_temperature": 0.0,
  "error_code": 0,
  "instantaneous_power": 468,
  "cumulative_energy": 272635,
  "current": 2.2,
  "voltage": 226
}
```

### Device Control

#### `POST /device/control`
Send control commands to a device.

**Request Body:**
```json
{
  "address": "20.00.00",
  "power": true,
  "mode": "cool",
  "target_temperature": 22.0,
  "fan_mode": "mid",
  "swing_vertical": true,
  "swing_horizontal": false,
  "preset": "quiet"
}
```

**Parameters:**

| Parameter | Type | Description | Values |
|-----------|------|-------------|--------|
| `address` | string | Device address | "XX.XX.XX" |
| `power` | boolean | Power on/off | `true`, `false` |
| `mode` | string\|int | Operating mode | "auto", "cool", "dry", "fan", "heat" or 0-4 |
| `target_temperature` | number | Target temperature | 16.0 - 30.0°C |
| `fan_mode` | string\|int | Fan speed | "auto", "low", "mid", "high", "turbo" or 0-4 |
| `swing_vertical` | boolean | Vertical swing | `true`, `false` |
| `swing_horizontal` | boolean | Horizontal swing | `true`, `false` |
| `preset` | string\|int | Preset mode | "none", "sleep", "quiet", "fast", "longreach", "eco", "windfree" or 0-9 |

**Mode Values:**
- `"auto"` / `0` - Auto mode
- `"cool"` / `1` - Cooling
- `"dry"` / `2` - Dehumidify
- `"fan"` / `3` - Fan only
- `"heat"` / `4` - Heating

**Fan Mode Values:**
- `"auto"` / `0` - Auto speed
- `"low"` / `1` - Low speed
- `"mid"` / `2` - Medium speed
- `"high"` / `3` - High speed
- `"turbo"` / `4` - Turbo speed

**Preset Values:**
- `"none"` / `0` - No preset
- `"sleep"` / `1` - Sleep mode
- `"quiet"` / `2` - Quiet operation
- `"fast"` / `3` - Fast cooling/heating
- `"longreach"` / `6` - Extended reach
- `"eco"` / `7` - Energy saving
- `"windfree"` / `9` - Samsung Wind-Free™ mode

**Response:**
```json
{
  "success": true
}
```

### Firmware Update

#### `GET /update`
Web interface for OTA firmware updates.

#### `POST /update`
Upload new firmware binary file.

**Content-Type:** `multipart/form-data`
**File field:** `firmware`
**File type:** `.bin` firmware file

## Device Types

The bridge automatically categorizes discovered devices:

| Address Range | Type | Description |
|---------------|------|-------------|
| `10.XX.XX` | Outdoor | Outdoor unit controllers |
| `20.XX.XX` | Indoor | Indoor unit controllers |
| `50.XX.XX` | WiredRemote | Wired remote controls |
| `62.XX.XX` | WiFiKit | WiFi communication modules |

## Protocol Details

This bridge implements the Samsung NASA (Network Air-conditioning System Architecture) protocol:

- **Serial communication:** 9600 baud, 8 data bits, even parity, 1 stop bit
- **Packet structure:** `[0x32][Size][SA][DA][Command][Messages][CRC16][0x34]`
- **CRC16 validation** for data integrity
- **Address-based routing** for multi-device networks
- **Multiple message types:** Enum, Variable, LongVariable, Structure

## Memory Optimization

Optimized for M5Stack Atom Lite's limited 520KB SRAM:

- **Small JSON buffers** (400-512 bytes)
- **LED display disabled** to save memory
- **Debug logging disabled** in production
- **Heap monitoring** and garbage collection
- **String pre-allocation** to reduce fragmentation

## Security Considerations

- **Change OTA password** from default `samsung123`
- **No built-in authentication** - secure network access appropriately
- **CORS enabled** - restrict origin in production if needed
- **Firmware verification** - only upload trusted firmware files

## Troubleshooting

### Common Issues

1. **No devices discovered**
   - Check RS485 wiring and connections
   - Ensure AC units are powered on
   - Verify baud rate (9600) and parity (even)

2. **WiFi connection problems**
   - Check SSID/password in source code
   - Verify signal strength with `/wifi` endpoint
   - Use mDNS hostname: `samsung-ac-bridge.local`

3. **Slow HTTP responses**
   - Check free heap with `/stats` endpoint
   - Monitor heap fragmentation
   - Restart device if memory is low

4. **OTA update failures**
   - Ensure firmware is valid `.bin` file
   - Check available flash memory (>50% free needed)
   - Verify network stability during upload

### Performance Monitoring

- **Heap usage:** Monitor with `/stats` endpoint
- **Request timing:** Each response includes `request_time_ms`
- **Loop performance:** Check `loop_time_ms` in system info
- **Memory leaks:** Watch `min_free_heap` over time

## License

This project is provided as-is for educational and personal use.