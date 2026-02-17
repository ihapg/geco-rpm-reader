// Adquisicion de datos

#include <Arduino.h>
#include <RPC.h>

#include "shared.h"

// === Variables ===
// Definicion de pines para los sensores
const uint8_t SENSOR_PINS[12] = {D0, D1, D2, D4, D5, D6, D7, D8, A2, D13, D14, D21};

// Tiempos de interrupcion
volatile unsigned long lastInterrupt[12];
volatile unsigned long interval[12];
volatile bool newData[12];

const unsigned long TIMEOUT = 12000;

// Estructura con datos
SensorData sensorDataM4;

static bool ledStatus = false;

// === Funciones ===
// Función bindeada por RPC para enviar datos a CM7
SensorData getData()
{
    return sensorDataM4;
}

// Función base para gestionar interrupciones
template <uint8_t id>
void baseInterruptionHandler()
{
    unsigned long now = micros();
    unsigned long diff = now - lastInterrupt[id];
    interval[id] = diff;
    lastInterrupt[id] = now;
    newData[id] = true;
}

void (*HANDLERS_LIST[12])() = {
    baseInterruptionHandler<0>, baseInterruptionHandler<1>, baseInterruptionHandler<2>, baseInterruptionHandler<3>,
    baseInterruptionHandler<4>, baseInterruptionHandler<5>, baseInterruptionHandler<6>, baseInterruptionHandler<7>,
    baseInterruptionHandler<8>, baseInterruptionHandler<9>, baseInterruptionHandler<10>, baseInterruptionHandler<11>};

// Función de actualización de sensores
void updateSensors()
{
    for (int i = 0; i < (sizeof(newData) / sizeof(newData[0])); i++)
    {
        if (newData[i])
        {
            // Copia del valor para asegurar dato durante operacion
            unsigned long intervalCopy = interval[i];
            newData[i] = false;
            if (intervalCopy > 0)
            {
                float sec = intervalCopy * 1e-6;
                sensorDataM4.hzs[i] = (1 / sec) * 10;
                sensorDataM4.rpms[i] = sensorDataM4.hzs[i] * 6;
                sensorDataM4.lastUpdate[i] = millis();
            }
        }
    }
}

// Función de comprobación de timeouts en sensores
void checkTimeouts()
{
    unsigned long now = millis();

    for (int i = 0; i < (sizeof(sensorDataM4.rpms) / sizeof(sensorDataM4.rpms[0])); i++)
    {
        if (sensorDataM4.rpms[i] > 0 && (now - sensorDataM4.lastUpdate[i]) > TIMEOUT)
        {
            sensorDataM4.rpms[i] = 0;
            sensorDataM4.hzs[i] = 0;
            sensorDataM4.lastUpdate[i] = now;
        }
    }
}

// Función de prueba RPC
bool getLed()
{
    return ledStatus;
}

// === Ejecución ===
void m4_setup()
{
    // Retardo para darle tiempo a RPC en CM7
    delay(200);

    RPC.begin();
    RPC.bind("get_data", getData);
    RPC.bind("test_led", getLed);

    pinMode(LEDB, OUTPUT);
    digitalWrite(LEDB, HIGH);

    // PINS de los sensores
    for (int i = 0; i < (sizeof(SENSOR_PINS) / sizeof(SENSOR_PINS[0])); i++)
    {
        pinMode(SENSOR_PINS[i], INPUT_PULLUP);
        int irq = digitalPinToInterrupt(SENSOR_PINS[i]);
        if (irq != NOT_AN_INTERRUPT)
        {
            attachInterrupt(irq, HANDLERS_LIST[i], RISING);
        }
        // gestionar error else/?
    }

    // Inicialización variables de datos
    unsigned long startMicros = micros();
    unsigned long startMillis = millis();

    for (int i = 0; i < 12; i++)
    {
        lastInterrupt[i] = startMicros;
        sensorDataM4.lastUpdate[i] = startMillis;
        sensorDataM4.rpms[i] = 0;
        sensorDataM4.hzs[i] = 0;
    }
}

void m4_loop()
{
    digitalWrite(LEDB, (millis() / 1000) % 2);

    updateSensors();
    checkTimeouts();
    delay(100);
}