#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h" // ESP-IDF v5.x Hardware Pulse Counter Driver

// Hardware Pin Definitions
#define GPIO_PWM_ENA    GPIO_NUM_19
#define GPIO_DIR_IN1    GPIO_NUM_18
#define GPIO_DIR_IN2    GPIO_NUM_5

#define GPIO_ENC_A      GPIO_NUM_34
#define GPIO_ENC_B      GPIO_NUM_35

#define KP              (100)
#define KI              (0)
#define KD              (0)

// LEDC PWM Configuration
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_10_BIT // Duty cycle range: 0 - 1023
#define LEDC_FREQUENCY  (10000)             // max 10 kHz PWM frequency for motor driver board

static pcnt_unit_handle_t pcnt_unit = NULL;

void init_motor_hardware(void)
{
    // 1. Configure Direction Pins (GPIO output)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_DIR_IN1) | (1ULL << GPIO_DIR_IN2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 2. Configure LEDC Timer for PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 3. Configure LEDC Channel for ENA PWM Output
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_PWM_ENA,
        .duty           = 0, // Initial duty 0 (off)
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void init_encoder_hardware(void)
{
    // 1. Setup PCNT Unit
    pcnt_unit_config_t unit_config = {
        .low_limit = -32768,
        .high_limit = 32767,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    // 2. Setup Glitch Filter (Ignores noise spikes under 1 microsecond)
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    // 3. Setup Channel A (Pulse input = Channel A, Control input = Channel B)
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = GPIO_ENC_A,
        .level_gpio_num = GPIO_ENC_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    // 4. Setup Channel B (Pulse input = Channel B, Control input = Channel A)
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = GPIO_ENC_B,
        .level_gpio_num = GPIO_ENC_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    // 5. Define Quadrature Counting Rules (1x / 4x resolution based on edge transitions)
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_INVERSE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));

    // 6. Enable and Start the Counter
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

int get_encoder_count(void)
{
    int pulse_count = 0;
    if (pcnt_unit != NULL) {
        pcnt_unit_get_count(pcnt_unit, &pulse_count);
    }
    return pulse_count;
}

// Set motor speed (0 to 1023)
void set_motor_speed(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// Direction Control Functions
void motor_forward(uint32_t speed_duty)
{
    gpio_set_level(GPIO_DIR_IN1, 1);
    gpio_set_level(GPIO_DIR_IN2, 0);
    set_motor_speed(speed_duty);
}

void motor_backward(uint32_t speed_duty)
{
    gpio_set_level(GPIO_DIR_IN1, 0);
    gpio_set_level(GPIO_DIR_IN2, 1);
    set_motor_speed(speed_duty);
}

void motor_stop(void)
{
    gpio_set_level(GPIO_DIR_IN1, 0);
    gpio_set_level(GPIO_DIR_IN2, 0);
    set_motor_speed(0);
}

void set_motor_speed_and_direction(int32_t duty) {
    if (duty > 0) {
        gpio_set_level(GPIO_DIR_IN1, 1);
        gpio_set_level(GPIO_DIR_IN2, 0);
    }
    else if (duty < 0) {
        gpio_set_level(GPIO_DIR_IN1, 0);
        gpio_set_level(GPIO_DIR_IN2, 1);
        duty = duty * -1;
    }
    else {
        gpio_set_level(GPIO_DIR_IN1, 0);
        gpio_set_level(GPIO_DIR_IN2, 0);
    }
    set_motor_speed(duty);
}

void encoder_logger_task(void *pvParameters)
{
    while (1) {
        printf("Encoder Ticks: %d\n", get_encoder_count());
        vTaskDelay(pdMS_TO_TICKS(200)); // Print count every 200ms
    }
}

int clamp(int input, int min, int max) {
    if (input > max) return max;
    else if (input < min) return min;
    else return input; 
}

float pid_compute(int target, int current, float *integral_accum, int *prev_error) {
    int error = target - current;

    // Proportional term
    float p_out = KP * (float)error;

    // Integral term
    *integral_accum += (float)error;
    float i_out = KI * (*integral_accum);

    // Derivative term
    float derivative = (float)(error - *prev_error);
    float d_out = KD * derivative;

    // Store previous error for next iteration
    *prev_error = error;

    // Calculate total output
    float total_output = p_out + i_out + d_out;

    // Clamp total output to PWM limits
    int clamped_output = clamp((int)total_output, -512, 512);

    return (float)clamped_output;
}

void move_to_position_task(void *pvParameters) {
    int32_t target_position = (int32_t)(uintptr_t)pvParameters;

    int current_position = 0;
    float integral_accum = 0.0f;
    int prev_error = 0;
    int settled_count = 0;

    printf("Moving to Target Position: %ld\n", target_position);

    while (1) {
        current_position = get_encoder_count();

        // Check if settled near target
        if (abs(target_position - current_position) <= 5) {
            settled_count++;
            if (settled_count >= 10) { // Must stay in tolerance for 100ms
                printf("Target reached! Stopping task.\n");
                break;
            }
        } else {
            settled_count = 0;
        }

        float pid_output = pid_compute(target_position, current_position, &integral_accum, &prev_error);
        set_motor_speed_and_direction((int)pid_output);

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Stop motor safety call
    set_motor_speed_and_direction(0);

    // FreeRTOS tasks must delete themselves when done
    vTaskDelete(NULL);
}

void app_main(void)
{
    printf("Initializing Motor Driver Test Routine...\n");
    init_motor_hardware();
    init_encoder_hardware();
    
    xTaskCreate(
        move_to_position_task,            
        "pid_move_task",                  
        4096,                             
        (void *)(uintptr_t)500,  
        5,                                
        NULL                              
    );

    vTaskDelay(pdMS_TO_TICKS(10000));

    xTaskCreate(
        move_to_position_task,            
        "pid_move_task",                  
        4096,                             
        (void *)(uintptr_t)1000,  
        5,                                
        NULL                              
    );

    vTaskDelay(pdMS_TO_TICKS(10000));

    xTaskCreate(
        move_to_position_task,            
        "pid_move_task",                  
        4096,                             
        (void *)(uintptr_t)0,  
        5,                                
        NULL                              
    );

    vTaskDelay(pdMS_TO_TICKS(10000));
}