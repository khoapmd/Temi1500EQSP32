# Temi1500ModbusASCIIESP32

This repository contains the source code for an ESP32-based temperature monitoring system using Modbus ASCII protocol. The project, named Temi1500ESP32, incorporates features such as MQTT communication, OTA updates, and secure configuration management.

## Features

- Modbus ASCII 7-Bits on Arduino Framework for old device communication
- MQTT data publishing and command reception
- Support Over-The-Air (OTA) updates and device management 
- Secure storage of sensitive configuration data
- Wi-Fi connectivity

## Project Structure

- `OTAHelper`: Manages Over-The-Air (OTA) updates
- `hex`: Handles hexadecimal conversions for Modbus ASCII
- `infoHelper`: Manages device information and configuration
- `main`: Contains the main program logic
- `mqttHelper`: Handles MQTT communication
- `uart`: Manages UART communication for Modbus

## Configuration

The project uses build flags to set important configuration parameters. Here's an explanation of each flag:

```ini
build_flags = 
    '-DAPPVERSION="1.0.0"'                        ; Current firmware version
    '-DAPPUPDNAME="temi1500ESP32"'                ; Name used for OTA updates
    '-DAPPENCKEY="your-encrypt-key"'              ; Encryption key for secure storage
    '-DAPPDEVTYPE="device-name"'                  ; Device type identifier
    '-DAPPAPIKEY="your-api-key"'                  ; API key for backend communication
    '-DAPPAPI="http://example.com/api"'           ; Backend API endpoint
    '-DAPPSSID="your-ssid"'                       ; Wi-Fi SSID
    '-DAPPPASSWORD="ssid-password"'               ; Wi-Fi password
    '-DAPPPMQTTSERVER="broker-address"'           ; MQTT broker address
    '-DAPPPMQTTUSER="mqtt-user"'                  ; MQTT username
    '-DAPPPMQTTPASSWORD="mqtt-password"'          ; MQTT password
    '-DAPPPMQTTDATATOPIC="/ESPChamber"'           ; MQTT topic for data publishing
    '-DAPPPMQTTSTSTOPIC="/ConnectStatus"'         ; MQTT topic for connection status
    '-DAPPPMQTTCMDTOPIC="/ESP32ChamberCMD"'       ; MQTT topic for receiving commands
```

## Setup and Usage

1. Clone this repository to your local machine.
2. Install PlatformIO in your preferred IDE (e.g., VS Code).
3. Open the project in PlatformIO.
4. Update the `build_flags` in `platformio.ini` with your specific configuration details.
5. Build and upload the project to your ESP32 device.

## Installation

1. **Wiring**:
   - Connect ESP32 device:
     - RO to GPIO 22
     - DI to GPIO 23
     - RE to GPIO 18
    
     ![img.png](https://github.com/khoapmd/Temi1500ModbusASCIIESP32/blob/main/imgs/ttl-rs485.png?raw=true)

2. **Firmware**:
   - Clone this repository
   - Open in PlatformIO or Arduino IDE
   - Adjust code for your setup if needed
   - Update the `build_flags` in `platformio.ini` with your specific configuration details
   - Flash to ESP32 device

3. **OTA Setup**:
   - Set up Flask-RESTX backend (see [Backend Repository](https://github.com/khoapmd/flask-restx-backend-controller))
   - Ensure backend URL in `platformio.ini` is correct

## Usage

1. Power on the LilyGo S3 board
2. System will:
   - Encrypt credentials (first boot only)
   - Connect to Wi-Fi
   - Read, check and sync with the server then apply configuration
   - Check for OTA updates
   - Begin publishing and receiving data to MQTT broker

## Dependencies

- ESP32 Arduino / espressif core
- 256dpi/MQTT
- ArduinoJson

## Development

To contribute to this project:

1. Fork the repository
2. Create a new branch for your feature
3. Commit your changes
4. Push to your fork
5. Submit a pull request

Please ensure you follow the existing code style and document any new features or changes.

## Troubleshooting

- If you encounter connectivity issues, double-check the Wi-Fi and MQTT credentials in the build flags.
- For OTA update problems, ensure that the `APPUPDNAME` matches the name set in your update server.
- Verify that the API endpoint (`APPAPI`) is correct and accessible.

## License

[Specify the license under which this project is released]

## Contact

For questions or support, please contact [Your Contact Information].
