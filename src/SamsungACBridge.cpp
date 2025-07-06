#include "SamsungACBridge.h"
#include "config.h"
#include <Arduino.h>

SamsungACBridge::SamsungACBridge() {
    serial = &Serial2; // Use Hardware Serial 2 for Samsung communication
}

SamsungACBridge::~SamsungACBridge() {
    // Cleanup if needed
}

void SamsungACBridge::begin(int rxPin, int txPin, unsigned long baudRate) {
    DEBUG_PRINTLN("Samsung AC Bridge initializing...");
    
    // Initialize UART for Samsung communication - Samsung AC uses Even parity
    serial->begin(baudRate, SERIAL_8E1, rxPin, txPin);
    serial->setTimeout(100);
    
    DEBUG_PRINTF("UART initialized on pins RX:%d TX:%d at %lu baud\n", rxPin, txPin, baudRate);
    
    rxBuffer.clear();
    devices.clear();
    discoveredAddresses.clear();
    
    DEBUG_PRINTLN("Samsung AC Bridge ready");
}

void SamsungACBridge::loop() {
    // Check for transmission timeout
    unsigned long now = millis();
    if (!rxBuffer.empty() && (now - lastTransmission >= TRANSMISSION_TIMEOUT_MS)) {
        DEBUG_PRINTLN("Transmission timeout - clearing buffer");
        rxBuffer.clear();
    }
    
    // Process command queue
    QueuedCommand* cmdToSend = commandQueue.getNextCommandToSend();
    if (cmdToSend) {
        // Send the command
        uint8_t seqNum = currentSequenceNumber++;
        if (currentSequenceNumber == 0) currentSequenceNumber = 1; // Skip 0
        
        // Convert QueuedRequest back to ProtocolRequest
        ProtocolRequest protocolReq;
        protocolReq.power = cmdToSend->request.power;
        protocolReq.hasPower = cmdToSend->request.hasPower;
        protocolReq.mode = (Mode)cmdToSend->request.mode;
        protocolReq.hasMode = cmdToSend->request.hasMode;
        protocolReq.targetTemperature = cmdToSend->request.targetTemperature;
        protocolReq.hasTargetTemperature = cmdToSend->request.hasTargetTemperature;
        protocolReq.fanMode = (FanMode)cmdToSend->request.fanMode;
        protocolReq.hasFanMode = cmdToSend->request.hasFanMode;
        protocolReq.swingVertical = cmdToSend->request.swingVertical;
        protocolReq.hasSwingVertical = cmdToSend->request.hasSwingVertical;
        protocolReq.swingHorizontal = cmdToSend->request.swingHorizontal;
        protocolReq.hasSwingHorizontal = cmdToSend->request.hasSwingHorizontal;
        protocolReq.preset = (Preset)cmdToSend->request.preset;
        protocolReq.hasPreset = cmdToSend->request.hasPreset;
        
        protocol.publishRequest(this, cmdToSend->targetAddress, protocolReq, seqNum);
        commandQueue.markCommandSent(cmdToSend, seqNum);
    }
    
    // Cleanup old commands
    static unsigned long lastCleanup = 0;
    if (now - lastCleanup > 5000) { // Every 5 seconds
        commandQueue.cleanup();
        lastCleanup = now;
    }
    
    // Read incoming data
    static unsigned long lastDebug = 0;
    if (serial->available() > 0 && (now - lastDebug > 5000)) {
        DEBUG_PRINTF("RS485 bytes available: %d\n", serial->available());
        lastDebug = now;
    }
    
    // Process max 64 bytes per iteration to avoid blocking
    int bytesToProcess = serial->available();
    if (bytesToProcess > 64) bytesToProcess = 64;
    
    while (bytesToProcess-- > 0 && serial->available()) {
        lastTransmission = now;
        uint8_t byte = serial->read();
        // Don't log individual bytes - too noisy
        // DEBUG_PRINTF("RX: 0x%02X\n", byte);
        
        // Skip until start byte found
        if (rxBuffer.empty() && byte != 0x32) {
            continue;
        }
        
        rxBuffer.push_back(byte);
    }
    
    // Try to process complete packet after reading
    if (!rxBuffer.empty()) {
        processData(rxBuffer);
    }
}

void SamsungACBridge::processData(std::vector<uint8_t>& data) {
    if (data.size() < 3) return; // Need at least start byte + 2 size bytes
    
    // Get expected packet size
    int expectedSize = ((int)data[1] << 8) | (int)data[2];
    expectedSize += 2; // Add size bytes themselves
    
    if ((int)data.size() < expectedSize) {
        return; // Wait for more data
    }
    
    // We have enough data, try to decode
    std::vector<uint8_t> packetData(data.begin(), data.begin() + expectedSize);
    
    DecodeResult result = tryDecodeNasaPacket(packetData);
    
    if (result == DecodeResult::Ok) {
        // DEBUG_PRINTLN("Valid NASA packet received");  // Too noisy, removed
        processNasaPacket(this);
        
        // Remove processed packet from buffer
        data.erase(data.begin(), data.begin() + expectedSize);
    } else {
        DEBUG_PRINTF("Packet decode failed: %d\n", (int)result);
        
        // Remove first byte and try again
        data.erase(data.begin());
    }
}

std::vector<String> SamsungACBridge::getDiscoveredDevices() {
    std::vector<String> result;
    for (const auto& address : discoveredAddresses) {
        result.push_back(address);
    }
    return result;
}

bool SamsungACBridge::isDeviceKnown(const String& address) {
    return discoveredAddresses.find(address) != discoveredAddresses.end();
}

bool SamsungACBridge::isDeviceOnline(const String& address) {
    auto it = devices.find(address);
    if (it == devices.end()) return false;
    
    unsigned long now = millis();
    return (now - it->second.lastUpdate) < DEVICE_TIMEOUT_MS_VALUE;
}

String SamsungACBridge::getDeviceType(const String& address) {
    // Parse address to determine type
    int dotIndex = address.indexOf('.');
    if (dotIndex == -1) return "Unknown";
    
    String klassStr = address.substring(0, dotIndex);
    uint8_t klass = strtol(klassStr.c_str(), nullptr, 16);
    
    if (klass == 0x10) return "Outdoor";
    else if (klass == 0x20) return "Indoor";
    else if (klass == 0x50) return "WiredRemote";
    else if (klass == 0x62) return "WiFiKit";
    else return "Other";
}

DeviceState SamsungACBridge::getDeviceState(const String& address) {
    auto it = devices.find(address);
    return (it != devices.end()) ? it->second : DeviceState();
}

bool SamsungACBridge::controlDevice(const String& address, const ControlRequest& request) {
    if (!isDeviceKnown(address)) {
        DEBUG_PRINTF("Device %s not known\n", address.c_str());
        return false;
    }
    
    // Convert ControlRequest to QueuedRequest
    QueuedRequest queuedRequest;
    
    if (request.hasPower) {
        queuedRequest.power = request.power;
        queuedRequest.hasPower = true;
    }
    
    if (request.hasMode) {
        queuedRequest.mode = (int)request.mode;
        queuedRequest.hasMode = true;
    }
    
    if (request.hasTargetTemperature) {
        queuedRequest.targetTemperature = request.targetTemperature;
        queuedRequest.hasTargetTemperature = true;
    }
    
    if (request.hasFanMode) {
        queuedRequest.fanMode = (int)request.fanMode;
        queuedRequest.hasFanMode = true;
    }
    
    if (request.hasSwingVertical) {
        queuedRequest.swingVertical = request.swingVertical;
        queuedRequest.hasSwingVertical = true;
    }
    
    if (request.hasSwingHorizontal) {
        queuedRequest.swingHorizontal = request.swingHorizontal;
        queuedRequest.hasSwingHorizontal = true;
    }
    
    if (request.hasPreset) {
        queuedRequest.preset = (int)request.preset;
        queuedRequest.hasPreset = true;
    }
    
    // Add command to queue instead of sending directly
    QueuedCommand* cmd = commandQueue.addCommand(address, queuedRequest);
    
    return cmd != nullptr;
}

// MessageTarget interface implementation
void SamsungACBridge::publishData(std::vector<uint8_t>& data) {
    DEBUG_PRINTF("TX: %d bytes to RS485\n", data.size());
    // Full hex dump is too noisy
    // DEBUG_PRINTF("Sending data: %s\n", bytesToHex(data).c_str());
    serial->write(data.data(), data.size());
    serial->flush();
}

void SamsungACBridge::registerAddress(const String& address) {
    if (discoveredAddresses.find(address) == discoveredAddresses.end()) {
        DEBUG_PRINTF("Discovered new device: %s (%s)\n", address.c_str(), getDeviceType(address).c_str());
        discoveredAddresses.insert(address);
    }
    updateDeviceState(address);
}

void SamsungACBridge::updateDeviceState(const String& address) {
    devices[address].lastUpdate = millis();
    
    // Check if any queued command is now confirmed
    DeviceState& state = devices[address];
    commandQueue.checkStateConfirmation(address, state.power, (int)state.mode, 
                                      state.targetTemperature, (int)state.fanMode, (int)state.preset);
}

void SamsungACBridge::setPower(const String& address, bool value) {
    // Only log if state actually changed
    if (devices[address].power != value) {
        DEBUG_PRINTF("Device %s power: %s\n", address.c_str(), value ? "ON" : "OFF");
    }
    devices[address].power = value;
    updateDeviceState(address);
}

void SamsungACBridge::setRoomTemperature(const String& address, float value) {
    // Room temp changes frequently, only log significant changes
    float oldValue = devices[address].roomTemperature;
    if (abs(oldValue - value) > 0.5) {
        DEBUG_PRINTF("Device %s room temperature: %.1f°C\n", address.c_str(), value);
    }
    devices[address].roomTemperature = value;
    updateDeviceState(address);
}

void SamsungACBridge::setTargetTemperature(const String& address, float value) {
    if (devices[address].targetTemperature != value) {
        DEBUG_PRINTF("Device %s target temperature: %.1f°C\n", address.c_str(), value);
    }
    devices[address].targetTemperature = value;
    updateDeviceState(address);
}

void SamsungACBridge::setOutdoorTemperature(const String& address, float value) {
    devices[address].outdoorTemperature = value;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s outdoor temperature: %.1f°C\n", address.c_str(), value);
}

void SamsungACBridge::setIndoorEvaInTemperature(const String& address, float value) {
    devices[address].evaInTemperature = value;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s eva in temperature: %.1f°C\n", address.c_str(), value);
}

void SamsungACBridge::setIndoorEvaOutTemperature(const String& address, float value) {
    devices[address].evaOutTemperature = value;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s eva out temperature: %.1f°C\n", address.c_str(), value);
}

void SamsungACBridge::setMode(const String& address, Mode mode) {
    devices[address].mode = mode;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s mode: %d\n", address.c_str(), (int)mode);
}

void SamsungACBridge::setFanMode(const String& address, FanMode fanmode) {
    devices[address].fanMode = fanmode;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s fan mode: %d\n", address.c_str(), (int)fanmode);
}

void SamsungACBridge::setSwingVertical(const String& address, bool vertical) {
    devices[address].swingVertical = vertical;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s swing vertical: %s\n", address.c_str(), vertical ? "ON" : "OFF");
}

void SamsungACBridge::setSwingHorizontal(const String& address, bool horizontal) {
    devices[address].swingHorizontal = horizontal;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s swing horizontal: %s\n", address.c_str(), horizontal ? "ON" : "OFF");
}

void SamsungACBridge::setPreset(const String& address, Preset preset) {
    devices[address].preset = preset;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s preset: %d\n", address.c_str(), (int)preset);
}

void SamsungACBridge::setCustomSensor(const String& address, uint16_t message_number, float value) {
    devices[address].customSensors[message_number] = value;
    updateDeviceState(address);
    // Note: Custom sensors are now only stored for messages we explicitly process
    // No logging here since all stored sensors are known and already logged in processMessageSet
}

void SamsungACBridge::setErrorCode(const String& address, int error_code) {
    devices[address].errorCode = error_code;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s error code: %d\n", address.c_str(), error_code);
}

void SamsungACBridge::setOutdoorInstantaneousPower(const String& address, float value) {
    devices[address].instantaneousPower = value;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s instantaneous power: %.1fW\n", address.c_str(), value);
}

void SamsungACBridge::setOutdoorCumulativeEnergy(const String& address, float value) {
    devices[address].cumulativeEnergy = value;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s cumulative energy: %.1fWh\n", address.c_str(), value);
}

void SamsungACBridge::setOutdoorCurrent(const String& address, float value) {
    devices[address].current = value;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s current: %.1fA\n", address.c_str(), value);
}

void SamsungACBridge::setOutdoorVoltage(const String& address, float value) {
    devices[address].voltage = value;
    updateDeviceState(address);
    DEBUG_PRINTF("Device %s voltage: %.1fV\n", address.c_str(), value);
}