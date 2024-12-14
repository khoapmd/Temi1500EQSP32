#ifndef MODBUS_HELPER_H
#define MODBUS_HELPER_H

#include <cstdint>
#include <stddef.h>

// Define the ChamberData struct
typedef struct {
    float tempPV;   // Temperature Process Value (D1)
    float tempSP;   // Temperature Set Point (D2)
    float wetPV;    // Wetness Process Value (D3)
    float wetSP;    // Wetness Set Point (D4)
    float humiPV;   // Humidity Process Value (D5)
    float humiSP;   // Humidity Set Point (D6)
    uint16_t nowSTS; // Current Status (D10)
} ChamberData;

void prepareModbusRequest(uint8_t* request, uint8_t slaveAddr, uint8_t functionCode, uint16_t startAddr, uint16_t regQuantity);
uint16_t calculateCRC(const uint8_t *data, size_t length);
float unsignedToSignedFloat(uint16_t value);
ChamberData readModbusResponse(uint8_t* response, size_t responseLength);

#endif