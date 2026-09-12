#ifndef DISPENSER_H
#define DISPENSER_H

#include <stdint.h>

/**
 * @brief DISPENSING: launching cards, IDLE: finished or waiting for command
 */
typedef enum {
    DISPENSING,
    IDLE
} dispenser_state_t;

/**
 * @brief initializes dispenser hardware
 */
void init_dispenser(void);

/**
 * @brief dispenses a specified number of cards
 * @param cards number of cards to be dispensed
 */
void dispense_card(int cards);

/**
 * @brief launches a task that dispenses a specified number of cards
 * @param cards number of cards to be dispensed
 */
void launch_card_task(int cards);

/**
 * @brief retrieves dispenser state
 * @return DISPENSING or IDLE
 */
dispenser_state_t get_dispenser_state(void);

#endif