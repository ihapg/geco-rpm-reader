// AUTORES: MIGUEL RODRÍGUEZ LÓPEZ y ADRIÁN PINEY GUTIÉRREZ
// Programa diseñado para medir las RPM y Hz de los agitadores del GeCo
// Versión: 8 -- SD (HTML + JS), DualCore (CM7 - CM4)

// === Imports === 
#include <Arduino.h>
#include "shared.h"

#ifdef CORE_CM7
  #include "m7/m7_web.h"
#endif

#ifdef CORE_CM4
  #include "m4/m4_acq.h"
#endif

// === Variables ===

// === Funciones ===

// === Ejecución ===
void setup() {
  #ifdef CORE_CM7
    // Se ejecuta el inicio de CM4 dentro de m7_setup()
    m7_setup();
  #endif

  #ifdef CORE_CM4
    m4_setup();
  #endif
}

void loop() {
  #ifdef CORE_CM7
    m7_loop();
  #endif

  #ifdef CORE_CM4
    m4_loop();
  #endif
}
