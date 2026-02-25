#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

void sendJsonResponse(EthernetClient& client, const JsonDocument& doc);
void handleSensors(EthernetClient& client);
void handleStatus(EthernetClient& client);
