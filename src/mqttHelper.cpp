#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <mqttHelper.h>
#include "debugSerial.h"

#define MSG_BUFFER_SIZE (50)

// Update these with values suitable for your network.
const char* ssid = APPSSID;
const char* password = APPPASSWORD;
const char* mqtt_server = APPPMQTTSERVER;
const char* mqtt_user = APPPMQTTUSER;
const char* mqtt_pass = APPPMQTTPASSWORD;

extern WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
int value = 0;

extern char boardID[23];

void setup_wifi() {
  delay(10);
  DebugSerial::print("Connecting to ");
  DebugSerial::println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DebugSerial::print(".");
  }
  
  randomSeed(micros());
  printWifiInfo();
}

void reconnect() {
  startWatchDog();
  
  // Ensure WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    DebugSerial::print(".");
    delay(500);
  }
  DebugSerial::println("Connected!");
  
  // Prepare will message
  String willMessageStr = "{\"status\":\"disconnected\", \"client\":\"" + String(boardID) + "\", \"appver\":\"" + String(APPVERSION) + "\"}";
  const char* willMessage = willMessageStr.c_str();
  
  // Loop until we're reconnected to the MQTT broker
  while (!mqttClient.connected()) {
    DebugSerial::print("Attempting MQTT connection...");
    
    // Attempt to connect with all parameters
    if (mqttClient.connect(boardID,                   // Client ID
                       mqtt_user,                 // Username
                       mqtt_pass,                 // Password
                       String(APPPMQTTSTSTOPIC).c_str(),  // Will Topic
                       2,                         // Will QoS
                       true,                      // Will Retain
                       willMessage,               // Will Message
                       true)) {                   // Clean Session
      DebugSerial::println("Connected!");
      
      // Send connection acknowledgment
      char dataToSend[72];
      snprintf(dataToSend, sizeof(dataToSend), 
               "{\"status\":\"%s\", \"client\": \"%s\", \"appver\": \"%s\"}", 
               "connected", boardID, String(APPVERSION));
      mqttClient.publish(String(APPPMQTTSTSTOPIC).c_str(), dataToSend, true);
      
      // Subscribe to command topics
      mqttClient.subscribe((String(APPPMQTTCMDTOPIC) + "/#").c_str());
    } else {
      DebugSerial::print("failed, rc=");
      DebugSerial::print(mqttClient.state());
      DebugSerial::println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String payloadStr;
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  
  DebugSerial::println("Incoming: " + String(topic) + " - " + payloadStr);
  
  // Check if the message is for this specific board or a global command
  if (String(topic) == String(APPPMQTTCMDTOPIC) || 
      String(topic) == String(APPPMQTTCMDTOPIC) + "/" + String(boardID)) {
    // Check if the message is "RESTART"
    if (payloadStr == "RESTART") {
      ESP.restart();
    }
  }
}

void setup_mqtt() {
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  mqttClient.setKeepAlive(60);
}

void mqttLoop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  
  esp_task_wdt_reset();
  mqttClient.loop();
}

void sendDataMQTT(ChamberData& data) {
  char dataToSend[256];
  snprintf(dataToSend, sizeof(dataToSend), 
           "{\"sensor_id\":\"%s\",\"tempPV\":%.2f,\"tempSP\":%.2f,\"wetPV\":%.2f,\"wetSP\":%.2f,\"humiPV\":%.2f,\"humiSP\":%.2f,\"nowSTS\":%d}", 
           boardID, data.tempPV, data.tempSP, data.wetPV, data.wetSP, data.humiPV, data.humiSP, data.nowSTS);
  
  mqttClient.publish(String(APPPMQTTDATATOPIC).c_str(), dataToSend);
  DebugSerial::println(dataToSend);
}