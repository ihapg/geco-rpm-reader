#pragma once

#include "shared.h"
#include "core/web_router.h"
#include "time/time_service.h"
#include "logging/sensor_log_writer.h"
#include "logging/logging_session.h"

#include "SDMMCBlockDevice.h"
#include "FATFileSystem.h"

void m7_setup();
void m7_loop();
LoggingSession& getLoggingSession();