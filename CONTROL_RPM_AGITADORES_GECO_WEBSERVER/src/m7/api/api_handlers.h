#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>

void sendJsonResponse(EthernetClient &client, const JsonDocument &doc);
void handleSensors(EthernetClient &client, String path);
void handleStatus(EthernetClient &client, String path);
void handleListLogs(EthernetClient &client, String path);
void handleDownloadLog(EthernetClient &client, String path);
void handleClearLogs(EthernetClient &client, String path);
void handleRenameLog(EthernetClient &client, String path);
