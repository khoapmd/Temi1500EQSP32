#define MAX_DATA_LENGTH 51  // Adjust based on your expected maximum message length
#define WDT_TIMEOUT 300 //5 minutes
#include <WiFi.h>

void startWatchDog();
void stopWatchDog();
void getData();
void getData2();
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void printWifiInfo();
void checkFirmware();
