#include "DebugWebSocket.h"
#include "DebugLog.h"

DebugStreamer debugStreamer;

// Implementation of DebugLog's broadcastToWebSocket method
void DebugLog::broadcastToWebSocket(const String& message) {
    debugStreamer.addMessage(message);
}