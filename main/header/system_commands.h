#ifndef SYSTEM_COMMANDS_H
#define SYSTEM_COMMANDS_H

#include <freertos/FREERTOS.h>
#include <freertos/task.h>

/**
 * @brief types of physical subsystem commands ran by the shuffle-deal mode button
 */
typedef enum {
    CMD_NONE = 0,
    CMD_START_SHUFFLE,
    CMD_STOP_SHUFFLE,
    CMD_START_DEAL,
    CMD_START_SHUFFLE_AND_DEAL
} sequencer_cmd_t;

extern TaskHandle_t xSequencerTaskHandle;

/**
 * @brief deals the card deck based on the config set in menu
 */
void command_system_task(void *pvParameters);

#endif