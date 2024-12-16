#include <Arduino.h>
#include <time.h>
#include <timeHelper.h>
#include "debugSerial.h"
// NTP server and timezone settings
const char* ntpServer = APPPNTPSERVER; // NTP server
const long gmtOffset_sec = 7*3600;    // GMT+7 offset in seconds
const int daylightOffset_sec = 3600;

void printLocalTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        DebugSerial::println("Failed to obtain time");
        return;
    }
    DebugSerial::println(&timeinfo);
}

void syncNTP() {
  // Configure NTP server and timezone
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  DebugSerial::println("Waiting for NTP time sync...");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    DebugSerial::println("Failed to obtain time");
    return;
  }
   DebugSerial::println("Time synchronized successfully!");
  printLocalTime();
}

String getUptime() {
  unsigned long millisSinceStart = millis();
  unsigned long seconds = millisSinceStart / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  // Format the uptime as "X days, HH:MM:SS"
  char uptimeString[64];
  snprintf(uptimeString, sizeof(uptimeString), "%lu days, %02lu:%02lu:%02lu", 
           days, hours % 24, minutes % 60, seconds % 60);

  return String(uptimeString);
}

String getDateTimeFromUptime(unsigned long uptimeSeconds) {
  // Get the current Unix timestamp (seconds since 1970)
  time_t now = time(nullptr);

  // Calculate the boot time by subtracting uptime from the current time
  time_t bootTime = now - uptimeSeconds;

  // Convert the boot time to a tm structure
  struct tm *timeinfo = localtime(&bootTime);

  // Format the date-time as a string (e.g., "YYYY-MM-DD HH:MM:SS")
  char dateTimeString[20];
  strftime(dateTimeString, sizeof(dateTimeString), "%Y-%m-%d %H:%M:%S", timeinfo);

  return String(dateTimeString);
}
