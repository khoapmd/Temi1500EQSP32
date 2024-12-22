#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "main.h"

#define MQTT_MAX_PACKET_SIZE 512 // NOTE: Have to edit the PubSubClient.h file, it rewrites the sketch
extern EQSP32 eqsp32;

extern TaskStackUsage stackUsageData;

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
String cmdTopic = String(APPPMQTTCMDTOPIC);
String dataTopic = String(APPPMQTTDATATOPIC);
String statusTopic = String(APPPMQTTSTSTOPIC);

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
  JsonDocument willJsonDoc;
  willJsonDoc["status"] = "disconnected";
  willJsonDoc["client"] = boardID;
  willJsonDoc["appver"] = APPVERSION;
  willJsonDoc["appScreenSize"] = APPSCREENSIZE;
  willJsonDoc["appUpdName"] = APPUPDNAME;
  willJsonDoc["appDevType"] = APPDEVTYPE;

  String willMessageStr;
  willJsonDoc.shrinkToFit();
  serializeJson(willJsonDoc, willMessageStr);
  const char *willMessage = willMessageStr.c_str();

  // Loop until we're reconnected to the MQTT broker
  while (!mqttClient.connected())
  {
    DebugSerial::print("Attempting MQTT connection...");

    // Attempt to connect with all parameters
    if (mqttClient.connect(boardID,           // Client ID
                           mqtt_user,         // Username
                           mqtt_pass,         // Password
                           statusTopic.c_str(), // Will Topic
                           2,                 // Will QoS
                           true,              // Will Retain
                           willMessage,       // Will Message
                           true))             // Clean Session
    {
      DebugSerial::println("MQTT Connected!");

      // Send connection acknowledgment
      JsonDocument connectJsonDoc;
      connectJsonDoc["status"] = "connected";
      connectJsonDoc["client"] = boardID;
      connectJsonDoc["ip"] = WiFi.localIP().toString();
      connectJsonDoc["appVersion"] = APPVERSION;
      connectJsonDoc["appScreenSize"] = APPSCREENSIZE;
      connectJsonDoc["appUpdName"] = APPUPDNAME;
      connectJsonDoc["appDevType"] = APPDEVTYPE;

      char dataToSend[256];
      connectJsonDoc.shrinkToFit();
      serializeJson(connectJsonDoc, dataToSend, sizeof(dataToSend));

      mqttClient.publish(statusTopic.c_str(), dataToSend, true);

      // Subscribe to command topics
      mqttClient.subscribe(cmdTopic.c_str());
      mqttClient.subscribe((cmdTopic + "/" + boardID).c_str());
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
  if (String(topic) == cmdTopic || String(topic) == (cmdTopic + "/" + boardID))
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
      JsonDocument statusJsonDoc;
      statusJsonDoc["client"] = boardID;
      statusJsonDoc["ip"] = WiFi.localIP().toString();
      statusJsonDoc["uptime"] = getUptime();
      statusJsonDoc["bootTime"] = getDateTimeFromUptime(millis() / 1000);
      statusJsonDoc["appVersion"] = APPVERSION;
      statusJsonDoc["appScreenSize"] = APPSCREENSIZE;
      statusJsonDoc["appUpdName"] = APPUPDNAME;
      statusJsonDoc["appDevType"] = APPDEVTYPE;
      // Add stack usage data
      JsonObject stackUsage = statusJsonDoc["stackUsage"].to<JsonObject>();
      stackUsage["modbusTask"] = stackUsageData.modbusTaskStack;
      stackUsage["firmwareTask"] = stackUsageData.firmwareTaskStack;
      stackUsage["ntpTask"] = stackUsageData.ntpTaskStack;
      statusJsonDoc["freeHeap"] = ESP.getFreeHeap();

      char dataToSend[512];
      statusJsonDoc.shrinkToFit();
      serializeJson(statusJsonDoc, dataToSend);

      // Publish the data
      bool publishResult = mqttClient.publish((cmdTopic + "/" + boardID).c_str(), dataToSend);
  
      if (!publishResult) {
        DebugSerial::println("MQTT Publish Failed!");
        DebugSerial::println("MQTT Client State: ");
        DebugSerial::println(mqttClient.state());
      }

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

void sendDataMQTT(const ChamberData &data)
{
  JsonDocument dataJsonDoc;
  dataJsonDoc["client"] = boardID;
  dataJsonDoc["tempPV"] = data.tempPV;
  dataJsonDoc["tempSP"] = data.tempSP;
  dataJsonDoc["wetPV"] = data.wetPV;
  dataJsonDoc["wetSP"] = data.wetSP;
  dataJsonDoc["humiPV"] = data.humiPV;
  dataJsonDoc["humiSP"] = data.humiSP;
  dataJsonDoc["nowSTS"] = data.nowSTS;

  char dataToSend[256];
  dataJsonDoc.shrinkToFit();
  serializeJson(dataJsonDoc, dataToSend, sizeof(dataToSend));

  mqttClient.publish(dataTopic.c_str(), dataToSend);
  DebugSerial::println(dataToSend);
}