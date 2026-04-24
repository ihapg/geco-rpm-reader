# GeCo RPM Reader WebServer

Repository for acquisition, control, and visualization of GeCo agitator RPM data.

## Overview

The main focus of this repository is the embedded firmware in [CONTROL_RPM_AGITADORES_GECO_WEBSERVER](CONTROL_RPM_AGITADORES_GECO_WEBSERVER), running on Portenta H7 with a dual-core design.

Its core responsibilities are:

- Real-time RPM/Hz acquisition from agitators.
- Device state handling and data logging.
- Network exposure of sensor data for external clients.
- Serving static web content from SD.

Other folders support validation, integration testing, and visualization.

## Main Project: Agitator Control

The primary component is [CONTROL_RPM_AGITADORES_GECO_WEBSERVER](CONTROL_RPM_AGITADORES_GECO_WEBSERVER).

### Dual-core responsibility split

- CM4 in [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m4](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m4): real-time signal acquisition and RPM/Hz computation.
- CM7 in [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7): server layer, routing, state logic, time services, and SD logging.

This separation keeps hard real-time acquisition isolated from network and service logic.

### Most relevant CM7 modules

- Routing/core: [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/core](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/core)
- API handlers: [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/api](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/api)
- Logging/session management: [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/logging](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/logging)
- Time synchronization/utilities: [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/time](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/time)

Firmware entry point: [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/main.cpp](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/main.cpp)

## Repository structure

- Main firmware: [CONTROL_RPM_AGITADORES_GECO_WEBSERVER](CONTROL_RPM_AGITADORES_GECO_WEBSERVER)
- Desktop test/template client: [Test_API](Test_API)
- SD-served static frontend: [SD_Files](SD_Files)

## Quick build with PlatformIO

Build environments are defined in [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/platformio.ini](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/platformio.ini):

- `m7` for CM7 firmware.
- `m4` for CM4 firmware.

Typical commands:

1. `platformio run -e m7`
2. `platformio run -e m4`
3. `platformio run -e m7 --target upload`
4. `platformio run -e m4 --target upload`

## About Test_API

[Test_API](Test_API) is a support project used to validate connectivity and runtime behavior from desktop, and it also acts as a reusable integration template.

It includes:

- A PyQt monitoring interface.
- An exportable HTTP client.
- A practical flow for status/sensor/log checks.

API details are documented in [Test_API/README.md](Test_API/README.md), so this root README intentionally does not go deep into endpoint-level documentation.

## Where to start

- To work on firmware behavior and server logic: start at [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/main.cpp](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/main.cpp) and [CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7](CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7).
- To validate integration from PC: start at [Test_API/main.py](Test_API/main.py).
- To review SD-served web assets: see [SD_Files/index.html](SD_Files/index.html) and [SD_Files/script.js](SD_Files/script.js).
