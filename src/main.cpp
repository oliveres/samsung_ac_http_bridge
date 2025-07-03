#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "NasaProtocol.h"
#include "SamsungACBridge.h"


// WiFi credentials - change these
const char* ssid = "AirPort";
const char* password = "skrinavesak";

// Web server on port 80
WebServer server(80);

// Samsung AC Bridge instance
SamsungACBridge bridge;

// Forward declarations
void setupOTA();
void setupRoutes();
void handleGetDevices();
void handleGetDevice();
void handleControlDevice();
void handleGetSensors();
void handleUpdatePage();
void handleUpdateUpload();
void handleUpdateFile();
void handleRS485Test();
void handleWiFiInfo();

// Helper functions for preset conversion
String presetToString(Preset preset) {
    switch (preset) {
        case Preset::None: return "none";
        case Preset::Sleep: return "sleep";
        case Preset::Quiet: return "quiet";
        case Preset::Fast: return "fast";
        case Preset::Longreach: return "longreach";
        case Preset::Eco: return "eco";
        case Preset::Windfree: return "windfree";
        default: return "unknown";
    }
}

Preset stringToPreset(const String& str) {
    if (str == "none") return Preset::None;
    if (str == "sleep") return Preset::Sleep;
    if (str == "quiet") return Preset::Quiet;
    if (str == "fast") return Preset::Fast;
    if (str == "longreach") return Preset::Longreach;
    if (str == "eco") return Preset::Eco;
    if (str == "windfree") return Preset::Windfree;
    return Preset::None;
}

void setup() {
    // Initialize M5Stack Atom Lite (disable LED display to save memory/power)
    M5.begin(true, false, false);  // SerialEnable, I2CEnable, DisplayEnable=false
    
    DEBUG_PRINTLN("Samsung AC HTTP Bridge starting...");
    
    DEBUG_PRINTLN("Initializing bridge...");
    // Initialize the bridge with correct Samsung AC settings
    // GPIO 22 (RX) and GPIO 19 (TX) for M5Stack Atom Lite
    bridge.begin(22, 19, 9600);  // RX=22, TX=19, 9600 baud with EVEN parity
    DEBUG_PRINTLN("Bridge initialized OK");
    
    DEBUG_PRINTLN("Starting WiFi...");
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        DEBUG_PRINTLN("Connecting to WiFi...");
    }
    DEBUG_PRINTLN("WiFi connected!");
    DEBUG_PRINT("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
    
    // Setup mDNS
    if (!MDNS.begin("samsung-ac-bridge")) {
        DEBUG_PRINTLN("Error setting up MDNS responder!");
    } else {
        DEBUG_PRINTLN("mDNS responder started: samsung-ac-bridge.local");
        MDNS.addService("http", "tcp", 80);
    }
    
    // Setup OTA
    setupOTA();
    
    // Setup HTTP routes
    setupRoutes();
    
    // Start web server
    server.begin();
    DEBUG_PRINTLN("HTTP server started");
    
}

void loop() {
    static unsigned long lastHeapCheck = 0;
    
    server.handleClient();
    ArduinoOTA.handle();
    bridge.loop();
    M5.update();  // Keep M5 alive
    
    // Periodic heap monitoring and cleanup
    if (millis() - lastHeapCheck > 30000) { // Every 30 seconds
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t minFreeHeap = ESP.getMinFreeHeap();
        
        if (freeHeap < 50000 || (freeHeap < minFreeHeap * 1.2)) {
            DEBUG_PRINTF("Low memory: free=%u, min=%u - forcing GC\n", freeHeap, minFreeHeap);
            // Force garbage collection
            yield();
            delay(1);
        }
        lastHeapCheck = millis();
    }
    
    yield();  // Let ESP32 handle background tasks
}

void setupOTA() {
    // ArduinoOTA setup
    ArduinoOTA.setHostname("samsung-ac-bridge");
    ArduinoOTA.setPassword("samsung123");
    
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        DEBUG_PRINTLN("Start updating " + type);
    });
    
    ArduinoOTA.onEnd([]() {
        DEBUG_PRINTLN("\nEnd");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        DEBUG_PRINTF("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            DEBUG_PRINTLN("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            DEBUG_PRINTLN("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            DEBUG_PRINTLN("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            DEBUG_PRINTLN("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            DEBUG_PRINTLN("End Failed");
        }
    });
    
    ArduinoOTA.begin();
    DEBUG_PRINTLN("ArduinoOTA ready");
    DEBUG_PRINTLN("Web OTA interface ready at /update");
}

void setupRoutes() {
    // CORS headers for all requests
    server.onNotFound([]() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server.send(404, "text/plain", "Not Found");
    });
    
    // Handle preflight OPTIONS requests
    server.on("/", HTTP_OPTIONS, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server.send(200);
    });
    
    // Root endpoint - system info
    server.on("/", HTTP_GET, []() {
        StaticJsonDocument<200> doc;
        doc["name"] = "Samsung AC HTTP Bridge";
        doc["version"] = "1.0.0";
        doc["uptime"] = millis() / 1000; // seconds
        doc["free_heap"] = ESP.getFreeHeap();
        
        String response;
        serializeJsonPretty(doc, response);
        
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", response);
    });
    
    // Get all discovered devices
    server.on("/devices", HTTP_GET, handleGetDevices);
    
    // Get device status
    server.on("/device", HTTP_GET, handleGetDevice);
    
    // Control device
    server.on("/device/control", HTTP_POST, handleControlDevice);
    
    // Get device sensors
    server.on("/device/sensors", HTTP_GET, handleGetSensors);
    
    // OTA Update endpoints
    server.on("/update", HTTP_GET, handleUpdatePage);
    server.on("/update", HTTP_POST, handleUpdateUpload, handleUpdateFile);
    
    // RS485 test endpoint
    server.on("/rs485test", HTTP_GET, handleRS485Test);
    
    // WiFi info endpoint
    server.on("/wifi", HTTP_GET, handleWiFiInfo);
    
}

void handleGetDevices() {
    // Smaller buffer for device list
    StaticJsonDocument<512> doc;
    JsonArray devices = doc.createNestedArray("devices");
    
    auto deviceList = bridge.getDiscoveredDevices();
    for (const auto& address : deviceList) {
        JsonObject device = devices.createNestedObject();
        device["address"] = address;
        device["type"] = bridge.getDeviceType(address);
        device["online"] = bridge.isDeviceOnline(address);
    }
    
    String response;
    serializeJsonPretty(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleGetDevice() {
    
    if (!server.hasArg("address")) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"error\":\"Missing address parameter\"}");
        return;
    }
    
    String address = server.arg("address");
    
    // Reduced JSON buffer size
    StaticJsonDocument<512> doc;
    
    if (!bridge.isDeviceKnown(address)) {
        doc["error"] = "Device not found";
        String response;
        serializeJsonPretty(doc, response);
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(404, "application/json", response);
        return;
    }
    
    // Get device state
    DeviceState state = bridge.getDeviceState(address);
    
    doc["address"] = address;
    doc["online"] = bridge.isDeviceOnline(address);
    doc["power"] = state.power;
    doc["mode"] = (int)state.mode;
    doc["target_temperature"] = round(state.targetTemperature * 10.0) / 10.0;
    doc["room_temperature"] = round(state.roomTemperature * 10.0) / 10.0;
    doc["fan_mode"] = (int)state.fanMode;
    doc["swing_vertical"] = state.swingVertical;
    doc["swing_horizontal"] = state.swingHorizontal;
    doc["preset"] = presetToString(state.preset);
    
    String response;
    response.reserve(768); // Pre-allocate to reduce fragmentation
    serializeJsonPretty(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleControlDevice() {
    if (!server.hasArg("plain")) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"error\":\"Missing JSON body\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (!doc.containsKey("address")) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"error\":\"Missing address field\"}");
        return;
    }
    
    String address = doc["address"];
    
    if (!bridge.isDeviceKnown(address)) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(404, "application/json", "{\"error\":\"Device not found\"}");
        return;
    }
    
    // Build control request
    ControlRequest request;
    
    if (doc.containsKey("power")) {
        request.power = doc["power"];
        request.hasPower = true;
    }
    
    if (doc.containsKey("mode")) {
        request.mode = (Mode)doc["mode"].as<int>();
        request.hasMode = true;
    }
    
    if (doc.containsKey("target_temperature")) {
        request.targetTemperature = doc["target_temperature"];
        request.hasTargetTemperature = true;
    }
    
    if (doc.containsKey("fan_mode")) {
        request.fanMode = (FanMode)doc["fan_mode"].as<int>();
        request.hasFanMode = true;
    }
    
    if (doc.containsKey("swing_vertical")) {
        request.swingVertical = doc["swing_vertical"];
        request.hasSwingVertical = true;
    }
    
    if (doc.containsKey("swing_horizontal")) {
        request.swingHorizontal = doc["swing_horizontal"];
        request.hasSwingHorizontal = true;
    }
    
    if (doc.containsKey("preset")) {
        if (doc["preset"].is<String>()) {
            request.preset = stringToPreset(doc["preset"].as<String>());
        } else {
            // Support legacy integer format
            request.preset = (Preset)doc["preset"].as<int>();
        }
        request.hasPreset = true;
    }
    
    // Send control request
    bool success = bridge.controlDevice(address, request);
    
    StaticJsonDocument<200> responseDoc;
    responseDoc["success"] = success;
    if (!success) {
        responseDoc["error"] = "Failed to send command";
    }
    
    String response;
    serializeJsonPretty(responseDoc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(success ? 200 : 500, "application/json", response);
}

void handleGetSensors() {
    if (!server.hasArg("address")) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"error\":\"Missing address parameter\"}");
        return;
    }
    
    String address = server.arg("address");
    
    if (!bridge.isDeviceKnown(address)) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(404, "application/json", "{\"error\":\"Device not found\"}");
        return;
    }
    
    // Smaller buffer for sensors
    StaticJsonDocument<400> doc;
    
    DeviceState state = bridge.getDeviceState(address);
    
    doc["address"] = address;
    doc["room_temperature"] = round(state.roomTemperature * 10.0) / 10.0;
    doc["target_temperature"] = round(state.targetTemperature * 10.0) / 10.0;
    doc["outdoor_temperature"] = round(state.outdoorTemperature * 10.0) / 10.0;
    doc["eva_in_temperature"] = round(state.evaInTemperature * 10.0) / 10.0;
    doc["eva_out_temperature"] = round(state.evaOutTemperature * 10.0) / 10.0;
    doc["error_code"] = state.errorCode;
    doc["instantaneous_power"] = state.instantaneousPower;
    doc["cumulative_energy"] = state.cumulativeEnergy;
    doc["current"] = state.current;
    doc["voltage"] = state.voltage;
    
    String response;
    serializeJsonPretty(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleUpdatePage() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Samsung AC Bridge - OTA Update</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background-color: #f5f5f5; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; margin-bottom: 30px; }
        .info { background: #e7f3ff; padding: 15px; border-radius: 5px; margin-bottom: 20px; border-left: 4px solid #2196F3; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }
        input[type="file"] { width: 100%; padding: 10px; border: 2px dashed #ddd; border-radius: 5px; background: #fafafa; }
        .upload-btn { background: #4CAF50; color: white; padding: 12px 30px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; width: 100%; }
        .upload-btn:hover { background: #45a049; }
        .upload-btn:disabled { background: #cccccc; cursor: not-allowed; }
        .progress { width: 100%; height: 20px; background: #f0f0f0; border-radius: 10px; overflow: hidden; margin-top: 10px; display: none; }
        .progress-bar { height: 100%; background: #4CAF50; width: 0%; transition: width 0.3s; }
        .status { margin-top: 15px; padding: 10px; border-radius: 5px; display: none; }
        .success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .warning { background: #fff3cd; color: #856404; border: 1px solid #ffeaa7; }
        a { color: #2196F3; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔄 Samsung AC Bridge OTA Update</h1>
        
        <div class="info">
            <strong>📋 Instructions:</strong><br>
            1. Build firmware using: <code>pio run</code><br>
            2. Find firmware file: <code>.pio/build/esp32dev/firmware.bin</code><br>
            3. Select the firmware.bin file below and click Update<br>
            4. Wait for the update to complete (device will restart automatically)
        </div>
        
        <form id="uploadForm" enctype="multipart/form-data">
            <div class="form-group">
                <label for="firmware">Select Firmware File (.bin):</label>
                <input type="file" id="firmware" name="firmware" accept=".bin" required>
            </div>
            
            <button type="submit" class="upload-btn" id="uploadBtn">
                📤 Upload & Update Firmware
            </button>
        </form>
        
        <div class="progress" id="progress">
            <div class="progress-bar" id="progressBar"></div>
        </div>
        
        <div class="status" id="status"></div>
        
        <br>
        <p style="text-align: center;">
            <a href="/">← Back to Main Page</a>
        </p>
    </div>

    <script>
        document.getElementById('uploadForm').onsubmit = function(e) {
            e.preventDefault();
            
            const fileInput = document.getElementById('firmware');
            const uploadBtn = document.getElementById('uploadBtn');
            const progress = document.getElementById('progress');
            const progressBar = document.getElementById('progressBar');
            const status = document.getElementById('status');
            
            if (!fileInput.files[0]) {
                showStatus('Please select a firmware file', 'error');
                return;
            }
            
            const file = fileInput.files[0];
            if (!file.name.endsWith('.bin')) {
                showStatus('Please select a .bin file', 'error');
                return;
            }
            
            const formData = new FormData();
            formData.append('firmware', file);
            
            uploadBtn.disabled = true;
            uploadBtn.textContent = '⏳ Uploading...';
            progress.style.display = 'block';
            status.style.display = 'none';
            
            const xhr = new XMLHttpRequest();
            
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const percent = (e.loaded / e.total) * 100;
                    progressBar.style.width = percent + '%';
                }
            };
            
            xhr.onload = function() {
                if (xhr.status === 200) {
                    progressBar.style.width = '100%';
                    showStatus('✅ Update successful! Device is restarting...', 'success');
                    setTimeout(() => {
                        showStatus('🔄 Please wait 30 seconds, then refresh the page', 'warning');
                    }, 3000);
                } else {
                    showStatus('❌ Update failed: ' + xhr.responseText, 'error');
                }
                uploadBtn.disabled = false;
                uploadBtn.textContent = '📤 Upload & Update Firmware';
            };
            
            xhr.onerror = function() {
                showStatus('❌ Upload error occurred', 'error');
                uploadBtn.disabled = false;
                uploadBtn.textContent = '📤 Upload & Update Firmware';
            };
            
            xhr.open('POST', '/update');
            xhr.send(formData);
        };
        
        function showStatus(message, type) {
            const status = document.getElementById('status');
            status.className = 'status ' + type;
            status.innerHTML = message;
            status.style.display = 'block';
        }
    </script>
</body>
</html>
)";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", html);
}

void handleUpdateUpload() {
    server.sendHeader("Connection", "close");
    
    if (Update.hasError()) {
        server.send(500, "text/plain", "Update failed");
    } else {
        server.send(200, "text/plain", "Update successful, restarting...");
        delay(1000);
        ESP.restart();
    }
}

void handleUpdateFile() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        DEBUG_PRINTF("Update Start: %s\n", upload.filename.c_str());
        
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            DEBUG_PRINTLN("Update begin failed");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            DEBUG_PRINTLN("Update write failed");
            Update.printError(Serial);
        } else {
            DEBUG_PRINTF("Update progress: %d bytes\n", upload.totalSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            DEBUG_PRINTF("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
        } else {
            DEBUG_PRINTLN("Update end failed");
            Update.printError(Serial);
        }
    }
}

void handleRS485Test() {
    StaticJsonDocument<500> doc;
    
    // RS485 port info
    doc["rx_pin"] = 22;
    doc["tx_pin"] = 19;
    doc["baud_rate"] = 9600;
    doc["parity"] = "EVEN";
    
    // Test if Serial2 is available
    doc["serial2_available"] = Serial2.available();
    
    // Test RS485 by sending a simple byte and checking echo
    Serial2.write(0xAA);  // Test byte
    delay(10);
    doc["bytes_sent"] = 1;
    doc["bytes_available_after_send"] = Serial2.available();
    
    // Read any available data
    String received = "";
    while (Serial2.available()) {
        uint8_t byte = Serial2.read();
        received += String(byte, HEX) + " ";
    }
    doc["received_data"] = received;
    
    String response;
    serializeJsonPretty(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleWiFiInfo() {
    StaticJsonDocument<500> doc;
    
    // WiFi connection status
    doc["connected"] = WiFi.isConnected();
    doc["ssid"] = WiFi.SSID();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet_mask"] = WiFi.subnetMask().toString();
    doc["dns"] = WiFi.dnsIP().toString();
    
    // Signal strength
    long rssi = WiFi.RSSI();
    doc["rssi"] = rssi;
    doc["signal_strength_dbm"] = rssi;
    
    // Convert RSSI to percentage (rough approximation)
    // RSSI typically ranges from -90 (worst) to -30 (best)
    int signalPercent = 0;
    if (rssi >= -30) {
        signalPercent = 100;
    } else if (rssi <= -90) {
        signalPercent = 0;
    } else {
        signalPercent = 2 * (rssi + 100);
    }
    doc["signal_strength_percent"] = signalPercent;
    
    // Signal quality description
    String quality;
    if (rssi >= -50) quality = "Excellent";
    else if (rssi >= -60) quality = "Good";
    else if (rssi >= -70) quality = "Fair";
    else if (rssi >= -80) quality = "Poor";
    else quality = "Very Poor";
    doc["signal_quality"] = quality;
    
    // Channel
    doc["channel"] = WiFi.channel();
    
    // Auto reconnect status
    doc["auto_reconnect"] = WiFi.getAutoReconnect();
    
    // Hostname
    doc["hostname"] = WiFi.getHostname();
    
    // Uptime
    doc["uptime_ms"] = millis();
    doc["uptime_seconds"] = millis() / 1000;
    
    String response;
    serializeJsonPretty(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}