#ifndef TURRET_H
#define TURRET_H

#include <stdio.h>
#include <stdint.h>

extern const int INITIAL_POSITION;

/**
 * @brief initializes turret hardware
 */
void init_turret(void);

/**
 * @brief get the motor encoder position
 * @return the motor encoder readingin ticks
 */
int get_encoder_count(void);

/**
 * @brief set the motor speed and direction
 * @param duty 0<->1023: FORWARD, -1023<->0: REVERSE, 0: STOP
 */
void set_motor_speed_and_direction(int32_t duty);

/**
 * @brief background task keep turret at its set target position
 * @param pvParameters initial target position at startup
 */
void turret_pid_task(void *pvParameters);

/**
 * @brief waits until motor is within target position
 * @param target_position target position in ticks
 */
void move_turret_to(int32_t target_position);

/**
 * @brief get the current target position for the turret motor
 * @return target position in ticks
 */
int get_target_position(void);

/**
 * @brief set the target position for the turret motor
 * @param pos target position in ticks
 */
void set_target_position(int pos);

#endif