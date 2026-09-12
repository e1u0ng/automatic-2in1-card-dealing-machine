#include "dispenser.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#define GPIO_SERVO_DISPENSER    GPIO_NUM_32

#define LEDC_MODE               LEDC_HIGH_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_14_BIT // 14-bit resolution (0 to 16383)
#define LEDC_FREQUENCY          50           // 50 Hz PWM frequency (20ms period)

// 14-bit duty values @ 50 Hz for 500us - 2500us pulse range
#define SERVO_MIN_PULSE_DUTY    410          // 500 us pulse (0 degrees)
#define SERVO_MID_PULSE_DUTY    1229         // 1500 us pulse (90 degrees)
#define SERVO_MAX_PULSE_DUTY    2048         // 2500 us pulse (180 degrees)
#define SERVO_SHOOT_DUTY        1790

dispenser_state_t dispenser_state = IDLE;

void init_dispenser(void) {
    // Configure LEDC timer 
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);   

    // Configure LEDC channel for dispenser servo
    ledc_channel_config_t ledc_channel2 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_2,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_SERVO_DISPENSER,
        .duty           = SERVO_SHOOT_DUTY, // Initial duty cycle
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel2);
}

void set_dispenser_servo_pos(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
}

void dispense_card(int cards) {
    dispenser_state = DISPENSING;
    for (int i = 0; i < cards; i++) {
        set_dispenser_servo_pos(SERVO_MAX_PULSE_DUTY); // resets
        vTaskDelay(pdMS_TO_TICKS(500));

        set_dispenser_servo_pos(SERVO_SHOOT_DUTY); // launches card
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    dispenser_state = IDLE;
}

void dispense_card_task(void *pvParameters) {
    int32_t cards = (int32_t)(uintptr_t)pvParameters;
    dispenser_state = DISPENSING;
    for (int i = 0; i < cards; i++) {
        set_dispenser_servo_pos(SERVO_MAX_PULSE_DUTY);
        vTaskDelay(pdMS_TO_TICKS(500));

        set_dispenser_servo_pos(SERVO_SHOOT_DUTY);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    dispenser_state = IDLE;
}

void launch_card_task(int cards) {
    xTaskCreate(dispense_card_task, "dispense", 4096, (void *)(uintptr_t)cards, 5, NULL);
}

dispenser_state_t get_dispenser_state(void) {
    return dispenser_state;
}

