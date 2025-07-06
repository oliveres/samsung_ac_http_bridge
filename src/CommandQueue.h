#pragma once

#include <Arduino.h>
#include <vector>
#include <memory>

// Forward declarations
struct ProtocolRequest;
struct DeviceState;

// Command states
enum class CommandState {
    Pending,        // Waiting to be sent
    Sent,           // Sent, waiting for ACK
    Acknowledged,   // ACK received
    Failed,         // Max retries exceeded
    Completed       // State change confirmed
};

// Simplified request for queue
struct QueuedRequest {
    bool power = false;
    bool hasPower = false;
    
    int mode = -1;
    bool hasMode = false;
    
    float targetTemperature = 0.0;
    bool hasTargetTemperature = false;
    
    int fanMode = -1;
    bool hasFanMode = false;
    
    bool swingVertical = false;
    bool hasSwingVertical = false;
    
    bool swingHorizontal = false;
    bool hasSwingHorizontal = false;
    
    int preset = 0;
    bool hasPreset = false;
};

// Single command in the queue
struct QueuedCommand {
    String targetAddress;
    QueuedRequest request;
    CommandState state;
    unsigned long sentTime;      // When last sent
    int retryCount;              // Number of retries
    uint8_t sequenceNumber;      // For matching ACK
    
    // Expected state after command execution
    struct ExpectedState {
        bool hasPower = false;
        bool power = false;
        
        bool hasMode = false;
        int mode = -1;
        
        bool hasTargetTemp = false;
        float targetTemp = 0;
        
        bool hasFanMode = false;
        int fanMode = -1;
        
        bool hasPreset = false;
        int preset = 0;
    } expectedState;
    
    QueuedCommand(const String& addr, const QueuedRequest& req) 
        : targetAddress(addr), request(req), state(CommandState::Pending), 
          sentTime(0), retryCount(0), sequenceNumber(0) {
        
        // Set expected state based on request
        if (req.hasPower) {
            expectedState.hasPower = true;
            expectedState.power = req.power;
        }
        if (req.hasMode) {
            expectedState.hasMode = true;
            expectedState.mode = req.mode;
        }
        if (req.hasTargetTemperature) {
            expectedState.hasTargetTemp = true;
            expectedState.targetTemp = req.targetTemperature;
        }
        if (req.hasFanMode) {
            expectedState.hasFanMode = true;
            expectedState.fanMode = req.fanMode;
        }
        if (req.hasPreset) {
            expectedState.hasPreset = true;
            expectedState.preset = req.preset;
        }
    }
};

// Command queue manager
class CommandQueue {
private:
    std::vector<std::unique_ptr<QueuedCommand>> commands;
    uint8_t nextSequenceNumber = 1;
    
    static const int MAX_RETRIES = 3;
    static const unsigned long ACK_TIMEOUT_MS = 1000;      // 1 second to receive ACK
    static const unsigned long RETRY_DELAY_MS = 500;       // 500ms between retries
    static const unsigned long STATE_CONFIRM_TIMEOUT_MS = 3000; // 3 seconds to see state change
    
public:
    // Add command to queue
    QueuedCommand* addCommand(const String& address, const QueuedRequest& request);
    
    // Process queue - returns command that needs to be sent
    QueuedCommand* getNextCommandToSend();
    
    // Mark command as sent
    void markCommandSent(QueuedCommand* cmd, uint8_t seqNum);
    
    // Handle received ACK
    void handleAck(uint8_t sequenceNumber);
    
    // Check if state matches expected for any command (using simplified state)
    void checkStateConfirmation(const String& address, bool power, int mode, 
                               float targetTemp, int fanMode, int preset);
    
    // Clean up old failed/completed commands
    void cleanup();
    
    // Get pending commands count
    size_t getPendingCount() const;
    
    // Check if any command is waiting for this address
    bool hasCommandsForAddress(const String& address) const;
};