#include <Arduino.h>
#include <time.h>
#include <timeHelper.h>
// NTP server and timezone settings
const char* ntpServer = APPPNTPSERVER; // NTP server
const long gmtOffset_sec = 7 * 3600;    // GMT+7 offset in seconds
const int daylightOffset_sec = 0;       // No daylight saving time

void setupNTP() {
  // Configure NTP server and timezone
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  unsigned long startTime = millis(); // Record the start time
  const unsigned long timeout = 10000; // Timeout in milliseconds (e.g., 10 seconds)
  // Wait for time synchronization
  Serial.println("Waiting for NTP time sync...");
  while (time(nullptr) < 8 * 3600) { // Wait until the time is synced
    if (millis() - startTime >= timeout) {
      Serial.println("NTP time sync timed out.");
      break; // Exit the loop if timeout is reached
    }
    delay(100);
  }

  if (time(nullptr) >= 8 * 3600) {
    Serial.println("NTP time sync complete.");
  }
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
