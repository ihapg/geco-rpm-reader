// Declaración de variables compartidas por ambos núcleos

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <rpc.h>

// Estructura de datos de sensores
struct SensorData {
    // uint32_t ids[];
    float rpms[12];
    float hzs[12];
    unsigned long lastUpdate[12];

    // Serializacion RPC
    MSGPACK_DEFINE_ARRAY(rpms, hzs);
};
