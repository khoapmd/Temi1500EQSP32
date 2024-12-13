#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <mqttHelper.h>
#include "OTAHelper.h"
#include "debugSerial.h"
#include <time.h>

#define MSG_BUFFER_SIZE (50)

// Update these with values suitable for your network.
const char *ssid = APPSSID;
const char *password = APPPASSWORD;
const char *mqtt_server = APPPMQTTSERVER;
const char *mqtt_user = APPPMQTTUSER;
const char *mqtt_pass = APPPMQTTPASSWORD;

WiFiClient espCustomClient;
PubSubClient mqttClient(espCustomClient);

unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
int value = 0;

extern char boardID[23];

String getUptime() {
  unsigned long millisSinceStart = millis();
  unsigned long seconds = millisSinceStart / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  // Format the uptime as "X days, HH:MM:SS"
  char uptimeString[64];
  snprintf(uptimeString, sizeof(uptimeString), "%lu days, %02lu:%02lu:%02lu", 
           days, hours % 24, minutes % 60, seconds % 60);

  return String(uptimeString);
}

String getDateTimeFromUptime(unsigned long uptimeSeconds) {
  // Get the current Unix timestamp (seconds since 1970)
  time_t now = time(nullptr);

  // Calculate the boot time by subtracting uptime from the current time
  time_t bootTime = now - uptimeSeconds;

  // Convert the boot time to a tm structure
  struct tm *timeinfo = localtime(&bootTime);

  // Format the date-time as a string (e.g., "YYYY-MM-DD HH:MM:SS")
  char dateTimeString[20];
  strftime(dateTimeString, sizeof(dateTimeString), "%Y-%m-%d %H:%M:%S", timeinfo);

  return String(dateTimeString);
}

void setup_wifi()
{
  delay(10);
  DebugSerial::print("Connecting to ");
  DebugSerial::println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    DebugSerial::print(".");
  }

  randomSeed(micros());
  printWifiInfo();
}

void reconnect()
{
  startWatchDog();

  // Ensure WiFi is connected
  while (WiFi.status() != WL_CONNECTED)
  {
    DebugSerial::print(".");
    delay(500);
  }
  DebugSerial::println("WiFi Connected!");

  // Prepare will message
  String willMessageStr = "{\"status\":\"disconnected\", \"client\":\"" + String(boardID) + "\", \"appver\":\"" + String(APPVERSION) + "\"}";
  const char *willMessage = willMessageStr.c_str();

  // Loop until we're reconnected to the MQTT broker
  while (!mqttClient.connected())
  {
    DebugSerial::print("Attempting MQTT connection...");

    // Attempt to connect with all parameters
    if (mqttClient.connect(boardID,                          // Client ID
                           mqtt_user,                        // Username
                           mqtt_pass,                        // Password
                           String(APPPMQTTSTSTOPIC).c_str(), // Will Topic
                           2,                                // Will QoS
                           true,                             // Will Retain
                           willMessage,                      // Will Message
                           true))
    { // Clean Session
      DebugSerial::println("MQTT Connected!");

      // Send connection acknowledgment
      char dataToSend[72];
      snprintf(dataToSend, sizeof(dataToSend),
               "{\"status\":\"%s\", \"client\": \"%s\", \"appver\": \"%s\"}",
               "connected", boardID, String(APPVERSION));
      mqttClient.publish(String(APPPMQTTSTSTOPIC).c_str(), dataToSend, true);

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
    if (payloadStr == "STATUS")
    {
      char dataToSend[256];
      String ipAddress = WiFi.localIP().toString();
      // Get the uptime in HH:MM:SS format
      String uptime = getUptime();
      String bootTime = getDateTimeFromUptime(millis() / 1000);
      snprintf(dataToSend, sizeof(dataToSend), 
           "{\"mac\":\"%s\",\"ip\":\"%s\",\"uptime\":\"%s\",\"bootTime\":\"%s\",\"appVersion\":\"%s\",\"appScreenSize\":\"%s\",\"appUpdName\":\"%s\",\"appDevType\":\"%s\"}", 
           boardID, ipAddress.c_str(), uptime.c_str(), bootTime.c_str(), APPVERSION, APPSCREENSIZE, APPUPDNAME, APPDEVTYPE);

      // Publish the data
      mqttClient.publish(String(APPPMQTTDATATOPIC).c_str(), dataToSend);

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

void sendDataMQTT(ChamberData &data)
{
  char dataToSend[256];
  snprintf(dataToSend, sizeof(dataToSend),
           "{\"sensor_id\":\"%s\",\"tempPV\":%.2f,\"tempSP\":%.2f,\"wetPV\":%.2f,\"wetSP\":%.2f,\"humiPV\":%.2f,\"humiSP\":%.2f,\"nowSTS\":%d}",
           boardID, data.tempPV, data.tempSP, data.wetPV, data.wetSP, data.humiPV, data.humiSP, data.nowSTS);

  mqttClient.publish(String(APPPMQTTDATATOPIC).c_str(), dataToSend);
  DebugSerial::println(dataToSend);
}