#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>
#include <map>
#include <set>
#include "NasaProtocol.h"

struct DeviceState {
    bool power = false;
    Mode mode = Mode::Unknown;
    float targetTemperature = 0.0;
    float roomTemperature = 0.0;
    float outdoorTemperature = 0.0;
    float evaInTemperature = 0.0;
    float evaOutTemperature = 0.0;
    FanMode fanMode = FanMode::Unknown;
    bool swingVertical = false;
    bool swingHorizontal = false;
    Preset preset = Preset::None;
    int errorCode = 0;
    float instantaneousPower = 0.0;
    float cumulativeEnergy = 0.0;
    float current = 0.0;
    float voltage = 0.0;
    unsigned long lastUpdate = 0;
    std::map<uint16_t, float> customSensors;
};

struct ProtocolRequest {
    bool power = false;
    bool hasPower = false;
    
    Mode mode = Mode::Unknown;
    bool hasMode = false;
    
    float targetTemperature = 0.0;
    bool hasTargetTemperature = false;
    
    FanMode fanMode = FanMode::Unknown;
    bool hasFanMode = false;
    
    bool swingVertical = false;
    bool hasSwingVertical = false;
    
    bool swingHorizontal = false;
    bool hasSwingHorizontal = false;
    
    Preset preset = Preset::None;
    bool hasPreset = false;
};

class MessageTarget {
public:
    virtual void publishData(std::vector<uint8_t>& data) = 0;
    virtual void registerAddress(const String& address) = 0;
    virtual void setPower(const String& address, bool value) = 0;
    virtual void setRoomTemperature(const String& address, float value) = 0;
    virtual void setTargetTemperature(const String& address, float value) = 0;
    virtual void setOutdoorTemperature(const String& address, float value) = 0;
    virtual void setIndoorEvaInTemperature(const String& address, float value) = 0;
    virtual void setIndoorEvaOutTemperature(const String& address, float value) = 0;
    virtual void setMode(const String& address, Mode mode) = 0;
    virtual void setFanMode(const String& address, FanMode fanmode) = 0;
    virtual void setSwingVertical(const String& address, bool vertical) = 0;
    virtual void setSwingHorizontal(const String& address, bool horizontal) = 0;
    virtual void setPreset(const String& address, Preset preset) = 0;
    virtual void setCustomSensor(const String& address, uint16_t message_number, float value) = 0;
    virtual void setErrorCode(const String& address, int error_code) = 0;
    virtual void setOutdoorInstantaneousPower(const String& address, float value) = 0;
    virtual void setOutdoorCumulativeEnergy(const String& address, float value) = 0;
    virtual void setOutdoorCurrent(const String& address, float value) = 0;
    virtual void setOutdoorVoltage(const String& address, float value) = 0;
};

struct ControlRequest {
    bool power = false;
    bool hasPower = false;
    
    Mode mode = Mode::Unknown;
    bool hasMode = false;
    
    float targetTemperature = 0.0;
    bool hasTargetTemperature = false;
    
    FanMode fanMode = FanMode::Unknown;
    bool hasFanMode = false;
    
    bool swingVertical = false;
    bool hasSwingVertical = false;
    
    bool swingHorizontal = false;
    bool hasSwingHorizontal = false;
    
    Preset preset = Preset::None;
    bool hasPreset = false;
};

class SamsungACBridge : public MessageTarget {
private:
    HardwareSerial* serial;
    std::vector<uint8_t> rxBuffer;
    std::map<String, DeviceState> devices;
    std::set<String> discoveredAddresses;
    NasaProtocol protocol;
    unsigned long lastTransmission = 0;
    
    // Performance monitoring
    unsigned long totalProcessingTime = 0;
    unsigned long requestCount = 0;
    
    static const unsigned long DEVICE_TIMEOUT_MS = 300000; // 5 minutes
    static const unsigned long TRANSMISSION_TIMEOUT_MS = 500;
    
public:
    SamsungACBridge();
    ~SamsungACBridge();
    
    void begin(int rxPin = 16, int txPin = 17, unsigned long baudRate = 2400);
    void loop();
    
    // Device discovery and management
    std::vector<String> getDiscoveredDevices();
    bool isDeviceKnown(const String& address);
    bool isDeviceOnline(const String& address);
    String getDeviceType(const String& address);
    
    // Device state
    DeviceState getDeviceState(const String& address);
    
    // Device control
    bool controlDevice(const String& address, const ControlRequest& request);
    
    // Performance monitoring
    float getAverageProcessingTime() { return requestCount > 0 ? (float)totalProcessingTime / requestCount : 0; }
    unsigned long getRequestCount() { return requestCount; }
    
    // MessageTarget interface implementation
    void publishData(std::vector<uint8_t>& data) override;
    void registerAddress(const String& address) override;
    void setPower(const String& address, bool value) override;
    void setRoomTemperature(const String& address, float value) override;
    void setTargetTemperature(const String& address, float value) override;
    void setOutdoorTemperature(const String& address, float value) override;
    void setIndoorEvaInTemperature(const String& address, float value) override;
    void setIndoorEvaOutTemperature(const String& address, float value) override;
    void setMode(const String& address, Mode mode) override;
    void setFanMode(const String& address, FanMode fanmode) override;
    void setSwingVertical(const String& address, bool vertical) override;
    void setSwingHorizontal(const String& address, bool horizontal) override;
    void setPreset(const String& address, Preset preset) override;
    void setCustomSensor(const String& address, uint16_t message_number, float value) override;
    void setErrorCode(const String& address, int error_code) override;
    void setOutdoorInstantaneousPower(const String& address, float value) override;
    void setOutdoorCumulativeEnergy(const String& address, float value) override;
    void setOutdoorCurrent(const String& address, float value) override;
    void setOutdoorVoltage(const String& address, float value) override;

private:
    void processData(std::vector<uint8_t>& data);
    void updateDeviceState(const String& address);
};

// Function declarations for protocol processing
void processMessageSet(String source, String dest, MessageSet& message, MessageTarget* target);