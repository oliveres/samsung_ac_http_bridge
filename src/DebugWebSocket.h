#pragma once

#include <Arduino.h>
#include <WString.h>
#include <vector>

// Simple SSE (Server-Sent Events) implementation for debug streaming
class DebugStreamer {
private:
    static const size_t MAX_LINE_LENGTH = 120;
    static const size_t MAX_BUFFER_SIZE = 100;  // Keep last 100 messages for new clients
    
    struct LogMessage {
        unsigned long timestamp;
        String message;
        
        LogMessage(const String& msg) : timestamp(millis()), message(msg) {}
    };
    
    std::vector<LogMessage> messageBuffer;
    unsigned long totalMessageCount = 0;
    
public:
    DebugStreamer() {}
    
    void addMessage(const String& message) {
        // Truncate if too long
        String truncated = message;
        if (truncated.length() > MAX_LINE_LENGTH) {
            truncated = truncated.substring(0, MAX_LINE_LENGTH - 3) + "...";
        }
        
        // Add to buffer
        messageBuffer.emplace_back(truncated);
        totalMessageCount++;
        
        // Keep buffer size manageable
        if (messageBuffer.size() > MAX_BUFFER_SIZE) {
            messageBuffer.erase(messageBuffer.begin());
        }
    }
    
    String getSSEStream() const {
        String stream = "";
        
        // Send buffered messages
        for (const auto& msg : messageBuffer) {
            unsigned long seconds = msg.timestamp / 1000;
            unsigned long millis_part = msg.timestamp % 1000;
            
            String data = "[" + String(seconds) + "." + String(millis_part) + "] " + msg.message;
            stream += "data: " + escapeSSE(data) + "\n\n";
        }
        
        return stream;
    }
    
    String getLatestSSE() const {
        if (messageBuffer.empty()) return "";
        
        const auto& msg = messageBuffer.back();
        unsigned long seconds = msg.timestamp / 1000;
        unsigned long millis_part = msg.timestamp % 1000;
        
        String data = "[" + String(seconds) + "." + String(millis_part) + "] " + msg.message;
        return "data: " + escapeSSE(data) + "\n\n";
    }
    
    void clear() {
        messageBuffer.clear();
    }
    
    size_t getMessageCount() const {
        return totalMessageCount;
    }
    
    String getMessagesJSON() const {
        String json = "[";
        
        bool first = true;
        for (const auto& msg : messageBuffer) {
            if (!first) json += ",";
            first = false;
            
            unsigned long seconds = msg.timestamp / 1000;
            unsigned long millis_part = msg.timestamp % 1000;
            
            json += "{\"timestamp\":\"[" + String(seconds) + "." + String(millis_part) + "]\",";
            json += "\"message\":\"" + escapeJSON(msg.message) + "\"}";
        }
        
        json += "]";
        return json;
    }
    
private:
    String escapeSSE(const String& text) const {
        String escaped = text;
        escaped.replace("\n", " ");
        escaped.replace("\r", " ");
        return escaped;
    }
    
    String escapeJSON(const String& text) const {
        String escaped = text;
        escaped.replace("\\", "\\\\");
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "\\n");
        escaped.replace("\r", "\\r");
        escaped.replace("\t", "\\t");
        return escaped;
    }
};

extern DebugStreamer debugStreamer;