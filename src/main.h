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

// typedef struct {
//     float tempPV;
//     float tempSP;
//     float wetPV;
//     float wetSP;
//     float humiPV;
//     float humiSP;
//     uint16_t nowSTS;
// } ChamberData;
// extern ChamberData chamberData;