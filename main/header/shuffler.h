#ifndef SHUFFLER_H
#define SHUFFLER_H

#include <stdint.h>

typedef enum {
    SHUFFLING,
    STOPPED
} shuffler_state_t;

void init_shuffler(void);
void start_shuffler(void);
void stop_shuffler(void);
void shuffle_for_time(int32_t ms);
shuffler_state_t get_shuffler_state(void);

#endif