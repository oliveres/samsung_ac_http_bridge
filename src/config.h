#pragma once

// M5Stack Atom Lite configuration
#include <M5Atom.h>
#include "DebugLog.h"
#include "DebugWebSocket.h"

// Debug output control
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
  #define DEBUG_PRINT(x) do { Serial.print(x); DebugLog::getInstance().addLine(String(x)); } while(0)
  #define DEBUG_PRINTLN(x) do { Serial.println(x); DebugLog::getInstance().addLine(String(x)); } while(0)
  #define DEBUG_PRINTF(...) do { Serial.printf(__VA_ARGS__); DebugLog::getInstance().printf(__VA_ARGS__); } while(0)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif