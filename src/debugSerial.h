#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

#include <Arduino.h>
#include <stdarg.h>

class DebugSerial {
public:
  // Enable/Disable debug mode
  static void setDebug(bool debug);
  
  // Get current debug state
  static bool getDebug();

  // Printf-style debug printing
  static void printf(const char* format, ...);
  static void printf_ln(const char* format, ...);

  // Standard print methods with type support
  template <typename T>
  static void print(T message) {
    if (debugEnabled) {
      Serial.print(message);
    }
  }

  template <typename T>
  static void println(T message) {
    if (debugEnabled) {
      Serial.println(message);
    }
  }

  // Overloaded print methods for multiple parameters
  template <typename T1, typename T2>
  static void print(T1 message1, T2 message2) {
    if (debugEnabled) {
      Serial.print(message1);
      Serial.print(message2);
    }
  }

  template <typename T1, typename T2>
  static void println(T1 message1, T2 message2) {
    if (debugEnabled) {
      Serial.print(message1);
      Serial.println(message2);
    }
  }

private:
  // Static debug flag
  static bool debugEnabled;
};

#endif // DEBUG_SERIAL_H