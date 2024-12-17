#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "infoHelper.h"
#include "debugSerial.h"

extern char boardID[23];

void checkDeviceExist()
{
    HTTPClient client;
    JsonDocument doc;
    String queryURL = String(APPAPI) + "/checkexist?u_id=" + String(boardID);
    DebugSerial::println("Will connect " + queryURL);
    client.begin(queryURL.c_str());
    client.addHeader("X-Secret-Key", String(APPAPIKEY));

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
            DebugSerial::println("Updating version in database");
            updateFirmver();
        }
        else DebugSerial::println("Version matching");
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
    String queryURL = String(APPAPI) + "/data?u_id=" + String(boardID);
    DebugSerial::println("Will connect " + queryURL);
    client.begin(queryURL.c_str());
    client.addHeader("Content-Type", "application/json");
    client.addHeader("X-Secret-Key", String(APPAPIKEY));
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
    String queryURL = String(APPAPI) + "/firmware?u_id=" + String(boardID);
    DebugSerial::println("Will connect " + queryURL);
    client.begin(queryURL.c_str());
    client.addHeader("X-Secret-Key", String(APPAPIKEY));
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