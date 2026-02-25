#pragma once

#include <Ethernet.h>

void processRequest(EthernetClient& client);
void serveFile(EthernetClient& client, const char* path, const char* mime);