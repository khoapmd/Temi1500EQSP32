#include <modbusHelper.h>
#include <string.h>
#include "debugSerial.h"

// Function to calculate CRC for Modbus RTU
uint16_t calculateCRC(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void printCRCDebug(uint8_t* response, size_t responseLength) {
    // Print the entire response
    Serial.print("Full Response: ");
    for (size_t i = 0; i < responseLength; i++) {
        Serial.print(response[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Calculate and print CRC of different message slices
    uint16_t fullMessageCRC = calculateCRC(response, responseLength - 2);
    uint16_t receivedCRC = (response[responseLength - 1] << 8) | response[responseLength - 2];

    Serial.print("Calculated CRC: ");
    Serial.println(fullMessageCRC, HEX);
    Serial.print("Received CRC: ");
    Serial.println(receivedCRC, HEX);
}

bool validateModbusCRC(uint8_t* response, size_t responseLength) {
    // Ensure enough bytes for CRC
    if (responseLength < 4) return false;

    // Calculate CRC on all bytes except the last two CRC bytes
    uint16_t calculatedCRC = calculateCRC(response, responseLength - 2);
    uint16_t receivedCRC = (response[responseLength - 1] << 8) | response[responseLength - 2];

    printCRCDebug(response, responseLength);  // Add this for detailed debugging

    return calculatedCRC == receivedCRC;
}

// Function to convert unsigned integer to signed float
float unsignedToSignedFloat(uint16_t value)
{
    // Interpret as a signed 16-bit value
    int16_t signedValue = static_cast<int16_t>(value);
    
    // Convert to float and divide by 100 to get decimal precision
    return static_cast<float>(signedValue) / 100.0;
}

// Function to prepare the Modbus RTU request
void prepareModbusRequest(uint8_t* request, uint8_t slaveAddr, uint8_t functionCode, uint16_t startAddr, uint16_t regQuantity) {
    // Prepare the request message
    request[0] = slaveAddr;                 // Slave address
    request[1] = functionCode;              // Function code (0x03 for Read Holding Registers)
    request[2] = (startAddr >> 8) & 0xFF;   // High byte of start address
    request[3] = startAddr & 0xFF;          // Low byte of start address
    request[4] = (regQuantity >> 8) & 0xFF; // High byte of register count
    request[5] = regQuantity & 0xFF;        // Low byte of register count

    // Calculate CRC
    uint16_t crc = calculateCRC(request, 6);
    request[6] = crc & 0xFF;        // Low byte of CRC
    request[7] = (crc >> 8) & 0xFF; // High byte of CRC
}

// Function to read and parse Modbus response
ChamberData readModbusResponse(uint8_t* response, size_t responseLength) {
    ChamberData chamberData;
    memset(&chamberData, 0, sizeof(chamberData)); // Initialize all fields to 0
    
    // Validate CRC
    if (responseLength < 5) { // Minimum response length for function code 0x03 is 5 bytes (slave address, function code, byte count, CRC)
        DebugSerial::println("Error: Response is too short.");
        return chamberData;
    }

    if (!validateModbusCRC(response, responseLength)) {
        DebugSerial::println("Error: CRC validation failed");
        return chamberData;
    }

    // Extract register values
    uint8_t dataBytesLength = response[2]; // Byte count (third byte in response)
    if (responseLength < 5 + dataBytesLength) {
        DebugSerial::println("Error: Response length does not match the byte count.");
        return chamberData;
    }

    uint8_t dataBytes[dataBytesLength];
    memcpy(dataBytes, &response[3], dataBytesLength); // Copy data bytes (skip slave address, function code, byte count, and CRC)

    // Map data to ChamberData struct
    chamberData.tempPV = unsignedToSignedFloat((dataBytes[0] << 8) | dataBytes[1]); // D1
    chamberData.tempSP = unsignedToSignedFloat((dataBytes[2] << 8) | dataBytes[3]); // D2
    chamberData.wetPV = unsignedToSignedFloat((dataBytes[4] << 8) | dataBytes[5]);  // D3
    chamberData.wetSP = unsignedToSignedFloat((dataBytes[6] << 8) | dataBytes[7]);  // D4
    chamberData.humiPV = unsignedToSignedFloat((dataBytes[8] << 8) | dataBytes[9]); // D5
    chamberData.humiSP = unsignedToSignedFloat((dataBytes[10] << 8) | dataBytes[11]);// D6
    chamberData.nowSTS = (dataBytes[18] << 8) | dataBytes[19];                             // D10

    return chamberData;
}