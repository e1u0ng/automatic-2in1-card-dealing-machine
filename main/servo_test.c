#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

// Hardware Definitions
#define SERVO_GPIO              33           // GPIO pin connected to the goBILDA injector signal
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          SERVO_GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_14_BIT // 14-bit resolution (0 to 16383)
#define LEDC_FREQUENCY          50           // 50 Hz PWM frequency (20ms period)

// 14-bit duty values @ 50 Hz for 500us - 2500us pulse range
#define SERVO_MIN_PULSE_DUTY    410          // 500 us pulse (0 degrees)
#define SERVO_MID_PULSE_DUTY    1229         // 1500 us pulse (90 degrees)
#define SERVO_MAX_PULSE_DUTY    2048         // 2500 us pulse (180 degrees)

// 14-bit duty values @ 50 Hz for 1000us - 200us pulse range
//#define SERVO_MIN_PULSE_DUTY    819          // 1000 us pulse (115 RPM CCW)
//#define SERVO_MID_PULSE_DUTY    1229         // 1500 us pulse (0 RPM)
//#define SERVO_MAX_PULSE_DUTY    1638         // 2000 us pulse (115 RPM CW)

void init_servo_pwm(void)
{
    // 1. Configure the LEDC Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_timer_config_t ledc_timer1 = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER_1,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer1);

    // 2. Configure the LEDC Channel on GPIO 17
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Initial duty cycle
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    ledc_channel_config_t ledc_channel1 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_1,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = 4,
        .duty           = 0, // Initial duty cycle
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel1);
}

void set_servo_duty_1(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void set_servo_duty_2(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
}

void app_main(void)
{
    printf("Initializing Servo PWM on GPIO %d...\n", SERVO_GPIO);
    init_servo_pwm();

    //set_servo_duty_2(1638);

    for (int i = 0; i < 3; i++) {
        printf("Moving Servo to 180 Degrees (Max Pulse)\n");
        set_servo_duty_1(SERVO_MAX_PULSE_DUTY);
        vTaskDelay(pdMS_TO_TICKS(1500));

        printf("Moving Servo to 0 Degrees (Min Pulse)\n");
        set_servo_duty_1(1500);
        vTaskDelay(pdMS_TO_TICKS(1500));

        //printf("Moving Servo to 90 Degrees (Mid Pulse)\n");
        //set_servo_duty(SERVO_MID_PULSE_DUTY);
        //vTaskDelay(pdMS_TO_TICKS(1500));
    }

    //set_servo_duty_2(1229);

    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);    
}

