// Declaración de variables compartidas por ambos núcleos

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <rpc.h>

// Estructura de datos de sensor individual
struct SensorDataInd
{
    uint32_t id;
    float rpm;
    float hz;
    unsigned long lastUpdate;

    // Serializacion RPC
    MSGPACK_DEFINE_ARRAY(id, rpm, hz, lastUpdate);
};

// Estructura de conjunto de sensores
struct SensorData
{
    // uint32_t ids[];
    // float rpms[12];
    // float hzs[12];
    // unsigned long lastUpdate[12];

    SensorDataInd sensors[12];

    // Serializacion RPC
    // MSGPACK_DEFINE_ARRAY(rpms, hzs);
    MSGPACK_DEFINE_ARRAY(sensors);
};
