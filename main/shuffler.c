#include "shuffler.h"
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#define GPIO_SERVO_SHUFFLER_1   GPIO_NUM_33         
#define GPIO_SERVO_SHUFFLER_2   GPIO_NUM_4

#define LEDC_MODE               LEDC_HIGH_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_14_BIT // 14-bit resolution (0 to 16383)
#define LEDC_FREQUENCY          50           // 50 Hz PWM frequency (20ms period)

// 14-bit duty values @ 50 Hz for 1000us - 200us pulse range
#define SERVO_MIN_PULSE_DUTY    819          // 1000 us pulse (115 RPM CCW)
#define SERVO_MID_PULSE_DUTY    1229         // 1500 us pulse (0 RPM)
#define SERVO_MAX_PULSE_DUTY    1638         // 2000 us pulse (115 RPM CW)

shuffler_state_t shuffler_state = STOPPED;

void init_shuffler(void) {
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer); 

    // Configure LEDC channel for left shuffler servo
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_SERVO_SHUFFLER_1,
        .duty           = SERVO_MID_PULSE_DUTY, // Initial duty cycle
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    // Configure LEDC channel for right shuffler servo
    ledc_channel_config_t ledc_channel1 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_SERVO_SHUFFLER_2,
        .duty           = SERVO_MID_PULSE_DUTY, // Initial duty cycle
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel1);
}

void set_left_shuffler_servo_speed(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
}

void set_right_shuffler_servo_speed(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
}

void start_shuffler(void) {
    if (shuffler_state == STOPPED) shuffler_state = SHUFFLING;
    set_left_shuffler_servo_speed(SERVO_MIN_PULSE_DUTY);
    vTaskDelay(pdMS_TO_TICKS(150));
    set_right_shuffler_servo_speed(SERVO_MIN_PULSE_DUTY);
}

void stop_shuffler(void) {
    if (shuffler_state == SHUFFLING) shuffler_state = STOPPED;
    set_left_shuffler_servo_speed(SERVO_MID_PULSE_DUTY);
    set_right_shuffler_servo_speed(SERVO_MID_PULSE_DUTY);
}

void shuffle_for_time(int32_t ms) {
    start_shuffler();
    vTaskDelay(pdMS_TO_TICKS(ms));
    stop_shuffler();
}

shuffler_state_t get_shuffler_state(void) {
    return shuffler_state;
}