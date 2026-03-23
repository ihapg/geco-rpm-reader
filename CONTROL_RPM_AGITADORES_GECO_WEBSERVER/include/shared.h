// Declaración de variables compartidas por ambos núcleos

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <rpc.h>

// Estructura de datos de sensor individual
struct SensorDataInd
{
    std::string id;
    float rpm;
    float hz;
    unsigned long lastUpdate;

    // Serializacion RPC
    MSGPACK_DEFINE_ARRAY(id, rpm, hz, lastUpdate);
};

// Estructura de conjunto de sensores
struct SensorData
{
    SensorDataInd sensors[12];

    // Serializacion RPC
    MSGPACK_DEFINE_ARRAY(sensors);
};
