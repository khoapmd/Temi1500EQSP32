#include <PubSubClient.h>
#include "main.h"

#define MQTT_MAX_PACKET_SIZE 512
extern EQSP32 eqsp32;
// Update these with values suitable for your network.
const char *ssid = APPSSID;
const char *password = APPPASSWORD;
const char *mqtt_server = APPPMQTTSERVER;
const char *mqtt_user = APPPMQTTUSER;
const char *mqtt_pass = APPPMQTTPASSWORD;

WiFiClient espCustomClient;
PubSubClient mqttClient(espCustomClient);

unsigned long lastMsg = 0;
int value = 0;

extern char boardID[23];

void reconnect()
{
  startWatchDog();
  // Ensure WiFi is connected
  while (!eqsp32.getWiFiStatus() == EQ_WF_CONNECTED && WiFi.localIP().toString() == "0.0.0.0")
  {
    DebugSerial::print(".");
    delay(500);
  }

  // Prepare will message
  String willMessageStr = "{\"status\":\"disconnected\", \"client\":\"" + String(boardID) + "\", \"appver\":\"" + String(APPVERSION) + "\", \"appScreenSize\":\"" + String(APPSCREENSIZE) + "\", \"appUpdName\":\"" + String(APPUPDNAME) + "\", \"appDevType\":\"" + String(APPDEVTYPE) + "\"}";
  const char *willMessage = willMessageStr.c_str();
  String fullTopic = String(APPPMQTTSTSTOPIC) + "/" + boardID;
  // Loop until we're reconnected to the MQTT broker
  while (!mqttClient.connected())
  {
    DebugSerial::print("Attempting MQTT connection...");
    
    // Attempt to connect with all parameters
    if (mqttClient.connect(boardID,                          // Client ID
                           mqtt_user,                        // Username
                           mqtt_pass,                        // Password
                           fullTopic.c_str(), // Will Topic
                           2,                                // Will QoS
                           true,                             // Will Retain
                           willMessage,                      // Will Message
                           true))
    { // Clean Session
      DebugSerial::println("MQTT Connected!");

      // Send connection acknowledgment
      char dataToSend[256];
      String ipAddress = WiFi.localIP().toString();
      snprintf(dataToSend, sizeof(dataToSend), 
           "{\"status\":\"connected\",\"client\":\"%s\",\"ip\":\"%s\",\"appVersion\":\"%s\",\"appScreenSize\":\"%s\",\"appUpdName\":\"%s\",\"appDevType\":\"%s\"}", 
           boardID, ipAddress.c_str(), APPVERSION, APPSCREENSIZE, APPUPDNAME, APPDEVTYPE);
      
      mqttClient.publish(fullTopic.c_str(), dataToSend, true);

      // Subscribe to command topics
      mqttClient.subscribe((String(APPPMQTTCMDTOPIC) + "/#").c_str());
    }
    else
    {
      DebugSerial::print("failed, rc=");
      DebugSerial::print(mqttClient.state());
      DebugSerial::println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  // Convert payload to string
  String payloadStr;
  for (unsigned int i = 0; i < length; i++)
  {
    payloadStr += (char)payload[i];
  }

  DebugSerial::println("Incoming: " + String(topic) + " - " + payloadStr);

  // Check if the message is for this specific board or a global command
  if (String(topic) == String(APPPMQTTCMDTOPIC) ||
      String(topic) == String(APPPMQTTCMDTOPIC) + "/" + String(boardID))
  {
    // Check if the message is "RESTART"
    if (payloadStr == "RESTART")
    {
      ESP.restart();
    }
    if (payloadStr == "UPDATE")
    {
      OTACheck(true);
    }
    if (payloadStr == "SYNCNTP")
    {
      syncNTP();
    }
    if (payloadStr == "STATUS")
    {
      char dataToSend[256];
      String ipAddress = WiFi.localIP().toString();
      // Get the uptime in HH:MM:SS format
      String uptime = getUptime();
      String bootTime = getDateTimeFromUptime(millis() / 1000);
      snprintf(dataToSend, sizeof(dataToSend), 
           "{\"client\":\"%s\",\"ip\":\"%s\",\"uptime\":\"%s\",\"bootTime\":\"%s\",\"appVersion\":\"%s\",\"appScreenSize\":\"%s\",\"appUpdName\":\"%s\",\"appDevType\":\"%s\"}", 
           boardID, ipAddress.c_str(), uptime.c_str(), bootTime.c_str(), APPVERSION, APPSCREENSIZE, APPUPDNAME, APPDEVTYPE);

      // Publish the data
      mqttClient.publish(String(APPPMQTTCMDTOPIC).c_str(), dataToSend);

      // Print the data to debug serial
      DebugSerial::println(dataToSend);
    }
  }
}

void setup_mqtt()
{
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  mqttClient.setKeepAlive(60);
}

void mqttLoop()
{
  if (!mqttClient.connected())
  {
    reconnect();
  }

  esp_task_wdt_reset();
  mqttClient.loop();
}

void sendDataMQTT(const ChamberData& data)
{
  char dataToSend[256];
  snprintf(dataToSend, sizeof(dataToSend),
           "{\"client\":\"%s\",\"tempPV\":%.2f,\"tempSP\":%.2f,\"wetPV\":%.2f,\"wetSP\":%.2f,\"humiPV\":%.2f,\"humiSP\":%.2f,\"nowSTS\":%d}",
           boardID, data.tempPV, data.tempSP, data.wetPV, data.wetSP, data.humiPV, data.humiSP, data.nowSTS);

  mqttClient.publish(String(APPPMQTTDATATOPIC).c_str(), dataToSend);
  DebugSerial::println(dataToSend);
}