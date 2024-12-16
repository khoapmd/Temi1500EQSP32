#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "infoHelper.h"
#include "debugSerial.h"
WiFiClient infoClient;

extern char boardID[23];

void checkDeviceExist()
{
    HTTPClient client;
    JsonDocument doc;
    client.addHeader("X-Secret-Key", String(APPAPIKEY));
    String queryURL = String(APPAPI) + "/checkexist?u_id=" + String(boardID);

    DebugSerial::println("Will connect " + queryURL);

    client.begin(infoClient, queryURL.c_str());

    int httpResponseCode = client.GET();

    if(httpResponseCode == 204) //code no content => info not exist
    {
        DebugSerial::println("code no content => info not exist");
        signInfo();
    }
    else if(httpResponseCode == 200){ //info exist, check firm_ver
        DebugSerial::println("info exist, check firm_ver on db");
        JsonDocument doc;
        deserializeJson(doc, client.getString());
        if(String(APPVERSION) != doc["firm_ver"]){
            updateFirmver();
        }
    }
    else{
        DebugSerial::print("HTTP Response code: ");
        DebugSerial::println(httpResponseCode);
        String payload = client.getString();
        DebugSerial::println("Response " + payload);
        
    }
    client.end();
}

void signInfo()
{
    HTTPClient client;
    client.addHeader("Content-Type", "application/json");
    client.addHeader("X-Secret-Key", String(APPAPIKEY));
    String queryURL = String(APPAPI) + "/data?u_id=" + String(boardID);
    DebugSerial::println("Will connect " + queryURL);
    client.begin(infoClient, queryURL.c_str());
    JsonDocument doc;
    doc["u_id"] = String(boardID);
    doc["device_type"] = String(APPDEVTYPE);
    doc["firm_ver"] = String(APPVERSION);

    String httpRequestData;
    serializeJson(doc, httpRequestData);
    DebugSerial::print(httpRequestData);
    int httpResponseCode = client.POST(httpRequestData);

    if (httpResponseCode > 0)
    {
        String response = client.getString();
        DebugSerial::println(httpResponseCode);
        DebugSerial::println(response);
    }
    else
    {
        DebugSerial::print("Error on sending POST: ");
        DebugSerial::println(httpResponseCode);
    }

    client.end();
}

void updateFirmver()
{
    HTTPClient client;
    client.addHeader("X-Secret-Key", String(APPAPIKEY));
    String queryURL = String(APPAPI) + "/firmware?u_id=" + String(boardID);
    client.begin(infoClient, queryURL.c_str());
    client.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["firm_ver"] = String(APPVERSION);

    String httpRequestData;
    serializeJson(doc, httpRequestData);
    DebugSerial::print(httpRequestData);
    int httpResponseCode = client.PUT(httpRequestData);

    if (httpResponseCode > 0)
    {
        String response = client.getString();
        DebugSerial::println(httpResponseCode);
        DebugSerial::println(response);
    }
    else
    {
        DebugSerial::print("Error on sending PUT: ");
        DebugSerial::println(httpResponseCode);
    }

    client.end();
}