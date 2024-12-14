#include <modbusHelper.h>

void setup_mqtt();
void reconnect();
void setup_wifi();
void mqttLoop();
void setWill();
void sendConnectionAck();
void sendDataMQTT(const ChamberData& chamberData);
void printMemoryUsage();