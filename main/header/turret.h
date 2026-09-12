#ifndef TURRET_H
#define TURRET_H

#include <stdio.h>
#include <stdint.h>

extern const int INITIAL_POSITION;

void init_turret(void);
int get_encoder_count(void);
void set_motor_speed_and_direction(int32_t duty);
void turret_pid_task(void *pvParameters);
void move_turret_to(int32_t target_position);
int get_target_position(void);
void set_target_position(int pos);

#endif