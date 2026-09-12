#ifndef DISPENSER_H
#define DISPENSER_H

#include <stdint.h>

typedef enum {
    DISPENSING,
    IDLE
} dispenser_state_t;

void init_dispenser(void);
void dispense_card(int cards);
void launch_card_task(int cards);
dispenser_state_t get_dispenser_state(void);

#endif