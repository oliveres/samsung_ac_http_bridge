#include "CommandQueue.h"
#include "config.h"

QueuedCommand* CommandQueue::addCommand(const String& address, const QueuedRequest& request) {
    auto cmd = std::unique_ptr<QueuedCommand>(new QueuedCommand(address, request));
    QueuedCommand* cmdPtr = cmd.get();
    commands.push_back(std::move(cmd));
    
    DEBUG_PRINTF("Command queued for %s, queue size: %d\n", address.c_str(), commands.size());
    return cmdPtr;
}

QueuedCommand* CommandQueue::getNextCommandToSend() {
    unsigned long now = millis();
    
    for (auto& cmd : commands) {
        if (!cmd) continue;
        
        switch (cmd->state) {
            case CommandState::Pending:
                // Ready to send immediately
                return cmd.get();
                
            case CommandState::Sent:
                // Check for ACK timeout
                if (now - cmd->sentTime > ACK_TIMEOUT_MS) {
                    if (cmd->retryCount < MAX_RETRIES) {
                        // Retry after delay
                        if (now - cmd->sentTime > ACK_TIMEOUT_MS + RETRY_DELAY_MS) {
                            DEBUG_PRINTF("Retrying command for %s (attempt %d/%d)\n", 
                                       cmd->targetAddress.c_str(), cmd->retryCount + 1, MAX_RETRIES);
                            cmd->state = CommandState::Pending;
                            return cmd.get();
                        }
                    } else {
                        // Max retries exceeded
                        DEBUG_PRINTF("Command failed for %s - max retries exceeded\n", cmd->targetAddress.c_str());
                        cmd->state = CommandState::Failed;
                    }
                }
                break;
                
            case CommandState::Acknowledged:
                // Check for state confirmation timeout
                if (now - cmd->sentTime > STATE_CONFIRM_TIMEOUT_MS) {
                    DEBUG_PRINTF("Command for %s acknowledged but state not confirmed\n", cmd->targetAddress.c_str());
                    cmd->state = CommandState::Completed;  // Consider it done anyway
                }
                break;
                
            default:
                break;
        }
    }
    
    return nullptr;
}

void CommandQueue::markCommandSent(QueuedCommand* cmd, uint8_t seqNum) {
    if (!cmd) return;
    
    cmd->state = CommandState::Sent;
    cmd->sentTime = millis();
    cmd->sequenceNumber = seqNum;
    cmd->retryCount++;
    
    DEBUG_PRINTF("Command sent to %s with seq %d\n", cmd->targetAddress.c_str(), seqNum);
}

void CommandQueue::handleAck(uint8_t sequenceNumber) {
    for (auto& cmd : commands) {
        if (cmd && cmd->state == CommandState::Sent && cmd->sequenceNumber == sequenceNumber) {
            DEBUG_PRINTF("ACK received for command to %s (seq %d)\n", 
                       cmd->targetAddress.c_str(), sequenceNumber);
            cmd->state = CommandState::Acknowledged;
            cmd->sentTime = millis();  // Reset timer for state confirmation
            return;
        }
    }
    
    DEBUG_PRINTF("ACK received for unknown sequence %d\n", sequenceNumber);
}

void CommandQueue::checkStateConfirmation(const String& address, bool power, int mode, 
                                        float targetTemp, int fanMode, int preset) {
    for (auto& cmd : commands) {
        if (!cmd || cmd->targetAddress != address || cmd->state != CommandState::Acknowledged) {
            continue;
        }
        
        bool stateMatches = true;
        
        // Check if current state matches expected state
        if (cmd->expectedState.hasPower && power != cmd->expectedState.power) {
            stateMatches = false;
        }
        if (cmd->expectedState.hasMode && mode != cmd->expectedState.mode) {
            stateMatches = false;
        }
        if (cmd->expectedState.hasTargetTemp && 
            abs(targetTemp - cmd->expectedState.targetTemp) > 0.1) {
            stateMatches = false;
        }
        if (cmd->expectedState.hasFanMode && fanMode != cmd->expectedState.fanMode) {
            stateMatches = false;
        }
        if (cmd->expectedState.hasPreset && preset != cmd->expectedState.preset) {
            stateMatches = false;
        }
        
        if (stateMatches) {
            DEBUG_PRINTF("State confirmed for command to %s\n", address.c_str());
            cmd->state = CommandState::Completed;
        }
    }
}

void CommandQueue::cleanup() {
    // Remove completed and failed commands older than 10 seconds
    unsigned long cutoffTime = millis() - 10000;
    
    commands.erase(
        std::remove_if(commands.begin(), commands.end(),
            [cutoffTime](const std::unique_ptr<QueuedCommand>& cmd) {
                if (!cmd) return true;
                return (cmd->state == CommandState::Completed || cmd->state == CommandState::Failed) &&
                       (cmd->sentTime < cutoffTime);
            }),
        commands.end()
    );
}

size_t CommandQueue::getPendingCount() const {
    size_t count = 0;
    for (const auto& cmd : commands) {
        if (cmd && (cmd->state == CommandState::Pending || cmd->state == CommandState::Sent)) {
            count++;
        }
    }
    return count;
}

bool CommandQueue::hasCommandsForAddress(const String& address) const {
    for (const auto& cmd : commands) {
        if (cmd && cmd->targetAddress == address && 
            (cmd->state == CommandState::Pending || cmd->state == CommandState::Sent || 
             cmd->state == CommandState::Acknowledged)) {
            return true;
        }
    }
    return false;
}