#ifndef SHUFFLER_H
#define SHUFFLER_H

#include <stdint.h>

/**
 * @brief SHUFFLING: on, STOPPED: off
 */
typedef enum {
    SHUFFLING,
    STOPPED
} shuffler_state_t;

/**
 * @brief initializes shuffler hardware
 */
void init_shuffler(void);

/**
 * @brief turns on shuffler indefinitely
 */
void start_shuffler(void);

/**
 * @brief turns off shuffler
 */
void stop_shuffler(void);

/**
 * @brief turns on shuffler for certain amount of time
 * @param ms time to run shuffler
 */
void shuffle_for_time(int32_t ms);

/**
 * @brief retrieves shuffler state
 * @return SHUFFLING or STOPPED
 */
shuffler_state_t get_shuffler_state(void);

#endif