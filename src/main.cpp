#include <Arduino.h>
#include "EQSP32.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>
#include "OTAHelper.h" // Include OTAHelper for firmware updates
#include "infoHelper.h"
#include "mqttHelper.h"
#include "debugSerial.h"

// Define RS-485 pins and baud rate
#define BAUD_RATE 115200

// EQSP32 instance
EQSP32 eqsp32;
char boardID[23];

// Semaphore for thread-safe access to shared resources
SemaphoreHandle_t xSemaphore = NULL;

// Function to calculate CRC for Modbus RTU
uint16_t calculateCRC(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Function to send Modbus RTU request
void modbusRequest(uint8_t slaveAddr, uint8_t functionCode, uint16_t startAddr, uint16_t regQuantity) {
    // Prepare the request message
    uint8_t request[8];
    request[0] = slaveAddr;                     // Slave address
    request[1] = functionCode;                  // Function code (0x03 for Read Holding Registers)
    request[2] = (startAddr >> 8) & 0xFF;       // High byte of start address
    request[3] = startAddr & 0xFF;              // Low byte of start address
    request[4] = (regQuantity >> 8) & 0xFF;     // High byte of register count
    request[5] = regQuantity & 0xFF;            // Low byte of register count

    // Calculate CRC
    uint16_t crc = calculateCRC(request, 6);
    request[6] = crc & 0xFF;                    // Low byte of CRC
    request[7] = (crc >> 8) & 0xFF;             // High byte of CRC

    // Send the request
    digitalWrite(eqsp32.getPin(EQ_RS485_EN), HIGH);       // Enable transmitter
    eqsp32.Serial.write(request, 8);            // Write the request to UART
    delay(10);                                  // Small delay
    digitalWrite(eqsp32.getPin(EQ_RS485_EN), LOW);        // Disable transmitter
}

// Function to convert unsigned integer to signed float
float unsignedToSignedFloat(uint16_t value) {
    if (value > 32767) {                        // Check if the value is greater than the maximum positive signed 16-bit value
        value -= 65536;                         // Convert to negative signed value
    }
    return static_cast<float>(value) / 10.0;   // Convert to float and divide by 10 (assuming scaling factor)
}

// Function to process Modbus response
void processModbusResponse() {
    if (eqsp32.Serial.available()) {
        uint8_t response[256];
        size_t responseLength = eqsp32.Serial.readBytes(response, 256);

        // Validate CRC
        uint16_t crc = calculateCRC(response, responseLength - 2);
        if (crc == (response[responseLength - 1] << 8) | response[responseLength - 2]) {
            DebugSerial::println("CRC Validated Successfully");

            // Extract register values
            uint8_t dataBytesLength = response[2]; // Byte count (third byte in response)
            uint8_t dataBytes[dataBytesLength];
            memcpy(dataBytes, &response[3], dataBytesLength); // Copy data bytes (skip slave address, function code, byte count, and CRC)

            DebugSerial::print("Register Values: ");
            for (uint8_t i = 0; i < dataBytesLength; i += 2) {
                uint16_t value = (dataBytes[i] << 8) | dataBytes[i + 1]; // Combine high and low bytes
                if (i < dataBytesLength - 2) {
                    value = unsignedToSignedFloat(value); // Convert D1 to D9 to signed float
                }
                DebugSerial::print(value);
                DebugSerial::print(" ");
            }
            DebugSerial::println("");
        } else {
            DebugSerial::println("CRC Validation Failed");
        }
    } else {
        DebugSerial::println("No Response Received");
    }
}

// Task to handle Modbus communication
void modbusTask(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            // Example: Read Holding Registers from D1 to D10 (Register Address 0 to 9)
            uint8_t slaveAddr = 1;       // Slave address
            uint8_t functionCode = 0x03; // Function code for Read Holding Registers
            uint16_t startAddr = 0;      // Start address of the holding register (D1 = 0, D2 = 1, ..., D10 = 9)
            uint16_t regQuantity = 10;   // Number of registers to read (D1 to D10)

            // Send Modbus request
            modbusRequest(slaveAddr, functionCode, startAddr, regQuantity);

            // Wait for response
            delay(200); // Wait for response
            processModbusResponse();

            xSemaphoreGive(xSemaphore); // Release the semaphore
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // Delay for 10 seconds before the next request
    }
}

// Task to check for firmware updates
void checkFirmwareTask(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            if (WiFi.status() == WL_CONNECTED && WiFi.localIP().toString() != "0.0.0.0") {
                DebugSerial::println("Checking for firmware updates...");
                OTACheck(true); // Call OTAHelper function to check for updates
            }
            xSemaphoreGive(xSemaphore); // Release the semaphore
        }
        vTaskDelay(pdMS_TO_TICKS(300000)); // Delay for 300 seconds (5 minutes)
    }
}

void setup() {
    // Initialize Serial Monitor for debugging
    Serial.begin(115200);

    // Initialize EQSP32
    EQSP32Configs configs;       
    configs.userDevName = boardID;
    configs.wifiSSID = APPSSID;
    configs.wifiPassword = APPPASSWORD;
    eqsp32.begin(configs, false);

    // Configure RS-485 pins
    pinMode(eqsp32.getPin(EQ_RS485_TX), OUTPUT);    // TX pin as output
    pinMode(eqsp32.getPin(EQ_RS485_RX), INPUT);     // RX pin as input
    pinMode(eqsp32.getPin(EQ_RS485_EN), OUTPUT);    // RS485 Enable pin as output
    digitalWrite(eqsp32.getPin(EQ_RS485_EN), LOW); // Default to receive mode

    // Configure UART for RS-485
    eqsp32.configSerial(RS485_TX, BAUD_RATE);
    eqsp32.configSerial(RS485_RX, BAUD_RATE);

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
    xTaskCreate(modbusTask, "ModbusTask", 4096, NULL, 1, NULL);
    xTaskCreate(checkFirmwareTask, "CheckFirmwareTask", 4096, NULL, 1, NULL);
}

void loop() {
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