#define MIN_TEMP_PV -80.0
#define MAX_TEMP_PV 160.0
#define MIN_TEMP_SP -80.0
#define MAX_TEMP_SP 160.0
#define MIN_HUMI_PV 0.0
#define MAX_HUMI_PV 100.0

#include <esp_task_wdt.h>
#include "main.h"
#include "mqttHelper.h"
#include "OTAHelper.h"
#include "infoHelper.h"
#include "EQSP32.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "debugSerial.h"

EQSP32 eqsp32;
char boardID[23];
// Ticker tickerFirmware(checkFirmware, 1800003, 0, MILLIS); // 30 minutes
const char *command = ":01030000000AF2\r\n"; // Get D0001 to D0010;
ChamberData chamberData;
bool newVersionChecked = false;
int retryCount = 0; // Counter for command sending retries
const int maxRetries = 3; // Maximum number of wrong data retries before taking action
const int maxEchoRetries = 300; // Maximum number of echo retries before taking action
int echoRetryCount = 0; // Counter for echo retries
SemaphoreHandle_t xSemaphore = NULL;

void getDataTask(void *pvParameters);
void checkFirmwareTask(void *pvParameters);

void setup()
{
    Serial.begin(115200); // Initialize Serial Monitor for debugging
    // uart_setup(UART_NUM_2, 115200, UART_DATA_7_BITS); //HardwareSerial does not support 7bit data in Arduino Framework, config in ESP-IDF Framework 
    // DebugSerial::println("UART2 configured for 7 data bits.");

    snprintf(boardID, 23, "%llX", ESP.getEfuseMac()); //Get unique ESP MAC
    // Initialize EQSP32
    EQSP32Configs configs;       
    configs.userDevName = boardID;
    configs.wifiSSID = APPSSID;
    configs.wifiPassword = APPPASSWORD;
    eqsp32.begin(configs, false);

    eqsp32.configSerial(RS485_TX, 115200); // Configure RS485 for 115200 baud
    
    //Wifi controls
    WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    
    startWatchDog(); //Start watch dog, if cannot connect to the wifi, esp will restart after 60 secs
    setup_wifi();
    setup_mqtt();
    checkDeviceExist();

    // Create a semaphore
    xSemaphore = xSemaphoreCreateBinary();
    if (xSemaphore != NULL) {
        xSemaphoreGive(xSemaphore); // Give the semaphore to start with
    }

    // Create tasks
    xTaskCreate(getDataTask, "getDataTask", 4096, NULL, 1, NULL);
    xTaskCreate(checkFirmwareTask, "checkFirmwareTask", 4096, NULL, 1, NULL);
}

void loop()
{
    mqttLoop();
    yield(); //prevent crash
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    printWifiInfo();
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    DebugSerial::println("Disconnected Event");
}

void startWatchDog()
{
    // WatchDog
    DebugSerial::println("Start WatchDog");
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
    esp_task_wdt_add(NULL);               // add current thread to WDT watch
}

void stopWatchDog()
{
    DebugSerial::println("Stop WatchDog");
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();
}

String readResponse(unsigned long timeout) {
    String response = "";
    unsigned long startTime = millis();

    while (millis() - startTime < timeout) {
        while (eqsp32.Serial.available()) {
            char byteChar = eqsp32.Serial.read();
            response += byteChar;
        }
    }

    return response.length() > 0 ? response : "Timeout";
}

void getDataTask(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            // Example: Sending a MODBUS RTU request to read D-Registers from D0000 to D0009 (10 registers)
            String hexCommand = "01030000000A700D"; // MODBUS RTU request to read D0000 to D0009
            eqsp32.Serial.write(hexCommand.c_str());

            // Example: Receive data over RS485 with a timeout
            String receivedData = readResponse(10); // 100ms timeout]

            if (receivedData != "Timeout" && receivedData != hexCommand) {
                DebugSerial::println("Received: " + receivedData);
                // Process the received data
                if (receivedData.length() >= 21) { // 1 byte for slave address, 1 byte for function code, 1 byte for byte count, 20 bytes for data (10 registers * 2 bytes each)
                    int byteCount = strtol(receivedData.substring(2, 4).c_str(), NULL, 16);
                    if (byteCount == 20) {
                        chamberData.tempPV = (float)strtol(receivedData.substring(4, 8).c_str(), NULL, 16);
                        chamberData.tempSP = (float)strtol(receivedData.substring(8, 12).c_str(), NULL, 16);
                        chamberData.wetPV = (float)strtol(receivedData.substring(12, 16).c_str(), NULL, 16);
                        chamberData.wetSP = (float)strtol(receivedData.substring(16, 20).c_str(), NULL, 16);
                        chamberData.humiPV = (float)strtol(receivedData.substring(20, 24).c_str(), NULL, 16);
                        chamberData.humiSP = (float)strtol(receivedData.substring(24, 28).c_str(), NULL, 16);
                        // chamberData.tempMVOut = (float)strtol(receivedData.substring(28, 32).c_str(), NULL, 16);
                        // chamberData.humiMVOut = (float)strtol(receivedData.substring(32, 36).c_str(), NULL, 16);
                        // chamberData.pidNo = (int)strtol(receivedData.substring(36, 40).c_str(), NULL, 16);
                        chamberData.nowSTS = (int)strtol(receivedData.substring(40, 44).c_str(), NULL, 16);

                        // Validate data
                        if (chamberData.tempPV >= MIN_TEMP_PV && chamberData.tempPV <= MAX_TEMP_PV &&
                            chamberData.tempSP >= MIN_TEMP_SP && chamberData.tempSP <= MAX_TEMP_SP &&
                            chamberData.humiPV >= MIN_HUMI_PV && chamberData.humiPV <= MAX_HUMI_PV &&
                            chamberData.humiSP >= MIN_HUMI_PV && chamberData.humiSP <= MAX_HUMI_PV) {
                            retryCount = 0; // Reset retry count on valid data
                            echoRetryCount = 0; // Reset echo retry count on valid data
                            sendDataMQTT(chamberData); 
                        } else {
                            retryCount++;
                            if (retryCount < maxRetries) {
                                DebugSerial::println("Invalid data received.");
                            } else {
                                DebugSerial::println("Consistent invalid data received. Resetting ESP...");
                                ESP.restart(); // Reset the ESP if invalid data persists
                            }
                        }
                    } else {
                        DebugSerial::println("Invalid byte count in response.");
                    }
                } else {
                    DebugSerial::println("Invalid response length.");
                }
            } else if (receivedData == hexCommand) {
                echoRetryCount++;
                if (echoRetryCount < maxEchoRetries) {
                    DebugSerial::println("Received command echo.");
                    chamberData.tempPV = 0;
                    chamberData.tempSP = 0;
                    chamberData.wetPV = 0;
                    chamberData.wetSP = 0;
                    chamberData.humiPV = 0;
                    chamberData.humiSP = 0;
                    chamberData.nowSTS = 0;
                    sendDataMQTT(chamberData); 
                } else {
                    DebugSerial::println("Repeated command echo detected. Resetting ESP...");
                    ESP.restart(); // Reset the ESP if repeated echoes persist
                }
            } else {
                DebugSerial::println("Timeout while reading data.");
            }

            vTaskDelay(pdMS_TO_TICKS(10000)); // Delay for 10 seconds
            xSemaphoreGive(xSemaphore); // Give the semaphore back
        }
    }
}

void checkFirmwareTask(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            if (WiFi.status() == WL_CONNECTED && WiFi.localIP().toString() != "0.0.0.0")
            {
                stopWatchDog();
                OTACheck(true);
                startWatchDog();
            }

            vTaskDelay(pdMS_TO_TICKS(300000)); // Delay for 300 seconds (5 minutes)
            xSemaphoreGive(xSemaphore); // Give the semaphore back
        }
    }
}

void printWifiInfo(){
    DebugSerial::println("");
    DebugSerial::println("WiFi connected");
    DebugSerial::print("Connected Network Signal Strength (RSSI): ");
    DebugSerial::println(WiFi.RSSI()); /*Print WiFi signal strength*/
    DebugSerial::print("Gateway: ");
    DebugSerial::println(WiFi.gatewayIP());
    DebugSerial::print("IP address: ");
    DebugSerial::println(WiFi.localIP());
}