#ifndef SYSTEM_COMMANDS_H
#define SYSTEM_COMMANDS_H

#include <freertos/FREERTOS.h>
#include <freertos/task.h>

typedef enum {
    CMD_NONE = 0,
    CMD_START_SHUFFLE,
    CMD_STOP_SHUFFLE,
    CMD_START_DEAL,
    CMD_START_SHUFFLE_AND_DEAL
} sequencer_cmd_t;

extern TaskHandle_t xSequencerTaskHandle;

void command_system_task(void *pvParameters);

#endif