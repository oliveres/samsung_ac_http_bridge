#pragma once

#include <Arduino.h>
#include <WString.h>

class DebugLog {
private:
    static const size_t MAX_LINE_LENGTH = 120;  // Max 120 chars per line
    bool enabled = true;
    
public:
    static DebugLog& getInstance() {
        static DebugLog instance;
        return instance;
    }
    
    void addLine(const String& message) {
        if (!enabled) return;
        
        // Prevent infinite loops from debug stream endpoint
        if (message.indexOf("Debug stream endpoint") >= 0) return;
        if (message.indexOf("debug-stream") >= 0) return;
        
        // Truncate if too long
        String truncated = message;
        if (truncated.length() > MAX_LINE_LENGTH) {
            truncated = truncated.substring(0, MAX_LINE_LENGTH - 3) + "...";
        }
        
        // Send to WebSocket instead of storing in buffer
        broadcastToWebSocket(truncated);
    }
    
    void printf(const char* format, ...) {
        if (!enabled) return;
        
        char buf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        
        addLine(String(buf));
    }
    
    String getHtml() const {
        String html = "<!DOCTYPE html><html><head>";
        html += "<title>Samsung AC Debug Console</title>";
        html += "<meta charset='utf-8'>";
        html += "<style>";
        html += "body { font-family: monospace; background: #1e1e1e; color: #d4d4d4; margin: 20px; }";
        html += ".console { background: #000; padding: 15px; border-radius: 5px; ";
        html += "height: 75vh; overflow-y: auto; white-space: pre-wrap; }";
        html += ".timestamp { color: #858585; }";
        html += ".message { color: #d4d4d4; }";
        html += ".header { color: #569cd6; margin-bottom: 10px; }";
        html += ".controls { margin-bottom: 10px; }";
        html += "button { background: #569cd6; color: white; border: none; ";
        html += "padding: 5px 15px; border-radius: 3px; cursor: pointer; }";
        html += "button:hover { background: #4d8cc7; }";
        html += ".status { margin-top: 10px; color: #858585; }";
        html += ".connected { color: #4ec9b0; }";
        html += ".disconnected { color: #f44747; }";
        html += "</style></head><body>";
        
        html += "<h2 class='header'>Samsung AC Bridge - Debug Console (Live)</h2>";
        html += "<div class='controls'>";
        html += "<button onclick='location.href=\"/\"'>System Info</button> ";
        html += "<button onclick='location.href=\"/devices\"'>Devices</button> ";
        html += "<button onclick='clearConsole()'>Clear Console</button>";
        html += "</div>";
        
        html += "<div class='console' id='console'>";
        html += "<div style='color: #858585;'>Connecting to live stream...</div>";
        html += "</div>";
        
        html += "<div class='status'>";
        html += "Status: <span id='status' class='disconnected'>Disconnected</span>";
        html += " | Free heap: " + String(ESP.getFreeHeap()) + " bytes";
        html += " | Messages: <span id='messageCount'>0</span>";
        html += "</div>";
        
        html += "<script>";
        html += "var messageCount = 0;";
        html += "var lastMessageCount = 0;";
        html += "var consoleEl = document.getElementById('console');";
        html += "var status = document.getElementById('status');";
        html += "var messageCountEl = document.getElementById('messageCount');";
        html += "";
        html += "function connect() {";
        html += "  console.log('Elements found:', status, consoleEl, messageCountEl);";
        html += "  status.textContent = 'Connecting...';";
        html += "  status.className = 'disconnected';";
        html += "  consoleEl.innerHTML = '<div style=\"color: #4ec9b0;\">Connecting to live debug stream...</div>';";
        html += "  lastMessageCount = -1;";
        html += "  fetchMessages();";
        html += "}";
        html += "";
        html += "function fetchMessages() {";
        html += "  fetch('/debug-stream')";
        html += "    .then(function(response) {";
        html += "      if (!response.ok) throw new Error('HTTP ' + response.status);";
        html += "      return response.json();";
        html += "    })";
        html += "    .then(function(data) {";
        html += "      console.log('Status check:', data.status);";
        html += "      if (data && data.status === 'ok') {";
        html += "        console.log('Setting status to Connected');";
        html += "        status.textContent = 'Connected';";
        html += "        status.className = 'connected';";
        html += "        ";
        html += "        if (data.count > lastMessageCount) {";
        html += "          consoleEl.innerHTML = '';";
        html += "          if (data.messages && data.messages.length > 0) {";
        html += "            for (var i = 0; i < data.messages.length; i++) {";
        html += "              addMessage(data.messages[i]);";
        html += "            }";
        html += "          }";
        html += "          lastMessageCount = data.count;";
        html += "          consoleEl.scrollTop = consoleEl.scrollHeight;";
        html += "        }";
        html += "        messageCount = data.count;";
        html += "        messageCountEl.textContent = messageCount;";
        html += "      } else {";
        html += "        status.textContent = 'Invalid Data';";
        html += "        status.className = 'disconnected';";
        html += "      }";
        html += "      ";
        html += "      while (consoleEl.children.length > 200) {";
        html += "        consoleEl.removeChild(consoleEl.firstChild);";
        html += "      }";
        html += "    })";
        html += "    .catch(function(error) {";
        html += "      console.log('Fetch error:', error);";
        html += "      status.textContent = 'Connection Error';";
        html += "      status.className = 'disconnected';";
        html += "    })";
        html += "    .finally(function() {";
        html += "      setTimeout(fetchMessages, 500);";
        html += "    });";
        html += "}";
        html += "";
        html += "function addMessage(msg) {";
        html += "  var div = document.createElement('div');";
        html += "  div.innerHTML = '<span class=\"timestamp\">' + escapeHtml(msg.timestamp) + '</span> <span class=\"message\">' + escapeHtml(msg.message) + '</span>';";
        html += "  consoleEl.appendChild(div);";
        html += "}";
        html += "";
        html += "function clearConsole() {";
        html += "  consoleEl.innerHTML = '';";
        html += "  messageCount = 0;";
        html += "  lastMessageCount = 0;";
        html += "  messageCountEl.textContent = messageCount;";
        html += "}";
        html += "";
        html += "function escapeHtml(text) {";
        html += "  var div = document.createElement('div');";
        html += "  div.textContent = text;";
        html += "  return div.innerHTML;";
        html += "}";
        html += "";
        html += "connect();";
        html += "</script>";
        html += "</body></html>";
        
        return html;
    }
    
    void clear() {
        // No buffer to clear in WebSocket mode
    }
    
    void setEnabled(bool enable) {
        enabled = enable;
    }
    
    bool isEnabled() const {
        return enabled;
    }
    
private:
    DebugLog() = default;
    
    void broadcastToWebSocket(const String& message);
};