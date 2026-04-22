#pragma once

#include "shared.h"
#include "web_router.h"
#include "time_service.h"
#include "sensor_log_writer.h"
#include "logging_session.h"

#include "SDMMCBlockDevice.h"
#include "FATFileSystem.h"

void m7_setup();
void m7_loop();
LoggingSession& getLoggingSession();