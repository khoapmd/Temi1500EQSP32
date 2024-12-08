#include "DebugSerial.h"

// Define the static member in the implementation file
bool DebugSerial::debugEnabled = false;

// Set debug mode
void DebugSerial::setDebug(bool debug) {
  debugEnabled = debug;
}

// Get current debug state
bool DebugSerial::getDebug() {
  return debugEnabled;
}

// Implement printf with debug flag
void DebugSerial::printf(const char* format, ...) {
  if (debugEnabled) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    Serial.print(buffer);
    va_end(args);
  }
}

// Implement printf with newline
void DebugSerial::printf_ln(const char* format, ...) {
  if (debugEnabled) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    Serial.println(buffer);
    va_end(args);
  }
}