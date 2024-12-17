#include <WiFi.h>
#include <Arduino.h>
#include "EQSP32.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>
#include "OTAHelper.h"
#include "mqttHelper.h"
#include "infoHelper.h"
#include "debugSerial.h"
#include "timeHelper.h"
#include "modbusHelper.h"

void startWatchDog();
void stopWatchDog();

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void printWifiInfo();
void checkFirmware();

struct TaskStackUsage {
    uint32_t modbusTaskStack;
    uint32_t firmwareTaskStack;
    uint32_t ntpTaskStack;
};