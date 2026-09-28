#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_ERROR = 2,
} log_level_t;

typedef struct {
    log_level_t level;
    char message[128];
    uint32_t tick_count;
} log_message_t;

typedef struct {
    sensor_snapshot_t telemetry;
    uint32_t seq;
} telemetry_message_t;

typedef struct {
    char command[32];
    bool enabled;
    uint32_t seq;
} command_message_t;

void firmware_task_init(void);

#ifdef __cplusplus
}
#endif
