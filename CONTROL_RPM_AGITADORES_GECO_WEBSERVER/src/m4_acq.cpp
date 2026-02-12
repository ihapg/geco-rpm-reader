// Implementacion Adquisicion de datos

#include <Arduino.h>
#include <RPC.h>

#include "shared.h"

// === Variables ===
SensorData sensorDataM4;

static bool ledStatus = false;
static uint32_t lastLEDToggle = 0;

SensorData getData()
{
    return sensorDataM4;
}

// Funcion de prueba RPC
bool getLed()
{
    return ledStatus;
}

void m4_setup()
{
    // Retardo para darle tiempo a RPC en CM7
    delay(200);

    RPC.begin();
    RPC.bind("get_data", getData);
    RPC.bind("test_led", getLed);

    pinMode(LEDB, OUTPUT);
    digitalWrite(LEDB, HIGH);

    lastLEDToggle = millis();
}

void m4_loop()
{
    digitalWrite(LEDB, (millis() / 1000) % 2);
    delay(500);
}