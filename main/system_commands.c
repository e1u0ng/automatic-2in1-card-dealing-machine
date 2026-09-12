#include "system_commands.h"
#include "shuffler.h"
#include "dispenser.h"
#include "turret.h"
#include "menu.h"

TaskHandle_t xSequencerTaskHandle = NULL;

void deal_cards(void) {
    // gets config values
    int players = get_num_player();
    int cards = get_cards_per_hand();
    int extra = get_extra_cards();

    // distribute cards
    for (int i = 0; i < cards; i++) {
        for (int j = 0; j < players; j++) {
            move_turret_to(INITIAL_POSITION + j * 50);
            dispense_card(1);
        }
    }

    // distribute extra card(s) if any
    if (extra != 0) {
        move_turret_to(25);
        dispense_card(extra);
    }

    // return to initial position
    move_turret_to(INITIAL_POSITION);
}

void command_system_task(void *pvParameters) {
    uint32_t command;

    while (1) {
        if (xTaskNotifyWait(0, ULONG_MAX, &command, portMAX_DELAY) == pdTRUE) { // waits until command is changed
            switch((sequencer_cmd_t)command) {
                case CMD_START_SHUFFLE: // hold button while in S mode
                    start_shuffler();
                    break;
                case CMD_STOP_SHUFFLE: // not holding button while in S mode
                    stop_shuffler();
                    break;
                case CMD_START_DEAL: // press button while in D mode
                    deal_cards();
                    break;
                case CMD_START_SHUFFLE_AND_DEAL: // press button while in SD mode
                    shuffle_for_time(4000);
                    deal_cards();
                    break;
                default:
                    break;
            }
        }
    }
}

