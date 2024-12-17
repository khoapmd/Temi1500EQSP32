#include "main.h"

#define BAUD_RATE 115200    // Define RS-485 baud rate
#define MODBUS_TIMEOUT 1000 // Timeout for Modbus response in milliseconds
#define MAX_DATA_LENGTH 51  // Adjust based on your expected maximum message length
#define WDT_TIMEOUT 300     // 5 minutes

// EQSP32 instance
EQSP32 eqsp32;
char boardID[23];

// Semaphore for thread-safe access to shared resources
SemaphoreHandle_t xSemaphore = NULL;
// Global task handles to track all tasks
TaskHandle_t modbusTaskHandle = NULL;
TaskHandle_t checkFirmwareTaskHandle = NULL;
TaskHandle_t syncNTPTaskHandle = NULL;

// Function to send Modbus RTU request
void modbusRequest(uint8_t slaveAddr, uint8_t functionCode, uint16_t startAddr, uint16_t regQuantity)
{
    // Prepare the request message
    uint8_t request[8];
    prepareModbusRequest(request, slaveAddr, functionCode, startAddr, regQuantity);
    // Send the request
    eqsp32.configSerial(RS485_TX, BAUD_RATE); // Enable transmitter
    eqsp32.Serial.write(request, 8);          // Write the request to UART
    eqsp32.Serial.flush();                    // Ensure message sent
    eqsp32.configSerial(RS485_RX, BAUD_RATE); // Disable transmitter
}

bool waitForModbusResponse(uint8_t *response, size_t *responseLength, uint32_t timeout)
{
    memset(response, 0, 256); // Clear the response buffer
    uint32_t startTime = millis();

    while (millis() - startTime < timeout)
    {
        if (eqsp32.Serial.available())
        {
            // Read the entire response into the buffer
            *responseLength = eqsp32.Serial.readBytes(response, 256);

            // Check if the response is at least as long as the request (8 bytes)
            if (*responseLength > 8)
            {
                // Remove the first 8 bytes (echo of the request)
                memmove(response, response + 8, *responseLength - 8);
                *responseLength -= 8;
                return true; // Response received
            }
        }
    }
    return false; // Timeout
}

// Task to handle Modbus communication
void modbusTask(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            // Example: Read Holding Registers from D1 to D10 (Register Address 0 to 9)
            uint8_t slaveAddr = 1;       // Slave address
            uint8_t functionCode = 0x03; // Function code for Read Holding Registers
            uint16_t startAddr = 0;      // Start address of the holding register (D1 = 0, D2 = 1, ..., D10 = 9)
            uint16_t regQuantity = 10;   // Number of registers to read (D1 to D10)

            // Send Modbus request
            modbusRequest(slaveAddr, functionCode, startAddr, regQuantity);
            uint8_t response[256];
            size_t responseLength = 0;
            ChamberData chamberData;
            memset(&chamberData, 0, sizeof(chamberData)); // Initialize all fields to 0
            if (waitForModbusResponse(response, &responseLength, MODBUS_TIMEOUT))
            {
                // Process the response
                chamberData = readModbusResponse(response, responseLength);
                for (size_t i = 0; i < responseLength; i++)
                {
                    Serial.print(response[i], HEX);
                    Serial.print(" ");
                }
            }
            else
            {
                DebugSerial::println("Modbus response timeout");
            }
            sendDataMQTT(chamberData);

            xSemaphoreGive(xSemaphore); // Release the semaphore
        }
        vTaskDelay(pdMS_TO_TICKS(60000)); // TODO: use the reference lib and read config from database
    }
}

// Task to check for firmware updates
void checkFirmwareTask(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            if (eqsp32.getWiFiStatus() == EQ_WF_CONNECTED && WiFi.localIP().toString() != "0.0.0.0")
            {
                DebugSerial::println("Checking for firmware updates...");
                OTACheck(true); // Call OTAHelper function to check for updates
            }
            xSemaphoreGive(xSemaphore); // Release the semaphore
        }
        vTaskDelay(pdMS_TO_TICKS(300000)); // Delay for 300 seconds (5 minutes)
    }
}

// Task to check for firmware updates
void syncNTPTask(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            if (eqsp32.getWiFiStatus() == EQ_WF_CONNECTED && WiFi.localIP().toString() != "0.0.0.0")
            {
                syncNTP();
            }
            xSemaphoreGive(xSemaphore); // Release the semaphore
        }
        vTaskDelay(pdMS_TO_TICKS(600000)); // Delay for 10 minutes
    }
}

void stackMonitorTask(void *pvParameters)
{
    while (1)
    {
        DebugSerial::println("--- TASK STACK USAGE REPORT ---");

        if (modbusTaskHandle != NULL)
            DebugSerial::printf("Modbus Task: %u bytes\n", uxTaskGetStackHighWaterMark(modbusTaskHandle));

        if (checkFirmwareTaskHandle != NULL)
            DebugSerial::printf("Firmware Task: %u bytes\n", uxTaskGetStackHighWaterMark(checkFirmwareTaskHandle));

        if (syncNTPTaskHandle != NULL)
            DebugSerial::printf("NTP Task: %u bytes\n", uxTaskGetStackHighWaterMark(syncNTPTaskHandle));

        // Overall system memory info
        DebugSerial::printf("Free Heap: %u bytes\n", ESP.getFreeHeap());

        vTaskDelay(pdMS_TO_TICKS(60000)); // Check every minute
    }
}

void setup()
{
    // Initialize Serial Monitor for debugging
    Serial.begin(115200);
    snprintf(boardID, 23, "%llX", ESP.getEfuseMac()); // Get unique ESP MAC
    // Initialize EQSP32
    EQSP32Configs configs;
    configs.devSystemID = boardID;
    configs.userDevName = boardID;
    configs.wifiSSID = APPSSID;
    configs.wifiPassword = APPPASSWORD;
    configs.relaySequencer = false;
    configs.mqttDiscovery = false;
    configs.customMobileApp = false;
    eqsp32.begin(configs, false);

    // Configure UART for RS-485
    eqsp32.configSerial(RS485_TX, BAUD_RATE);
    eqsp32.configSerial(RS485_RX, BAUD_RATE);

    startWatchDog(); // Start watch dog, if cannot connect to the wifi, esp will restart after 60 secs
    // setup_wifi(); //Handled by EQSP32
    while (!eqsp32.getWiFiStatus() == EQ_WF_CONNECTED && WiFi.localIP().toString() == "0.0.0.0")
    {
        delay(500);
        DebugSerial::print(".");
    }
    setup_mqtt();
    checkDeviceExist();

    // Create a semaphore
    xSemaphore = xSemaphoreCreateBinary();
    if (xSemaphore != NULL)
    {
        xSemaphoreGive(xSemaphore); // Give the semaphore to start with
    }

    // Create tasks
    xTaskCreate(modbusTask, "ModbusTask", 4096, NULL, 1, &modbusTaskHandle);
    xTaskCreate(checkFirmwareTask, "CheckFirmwareTask", 8192, NULL, 1, &checkFirmwareTaskHandle);
    xTaskCreate(syncNTPTask, "syncNTPTask", 4096, NULL, 1, &syncNTPTaskHandle);
    xTaskCreate(stackMonitorTask, "StackMonitorTask", 4096, NULL, 1, NULL);
}

void loop()
{
    mqttLoop();
    yield(); // prevent crash
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

void printWifiInfo()
{
    DebugSerial::println("");
    DebugSerial::println("WiFi connected");
    DebugSerial::print("Connected Network Signal Strength (RSSI): ");
    DebugSerial::println(WiFi.RSSI()); /*Print WiFi signal strength*/
    DebugSerial::print("Gateway: ");
    DebugSerial::println(WiFi.gatewayIP());
    DebugSerial::print("IP address: ");
    DebugSerial::println(WiFi.localIP());
}