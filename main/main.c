#include <stdio.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/pulse_cnt.h>
#include <driver/i2c_master.h>

#include "shuffler.h"
#include "dispenser.h"
#include "turret.h"
#include "menu.h"
#include "system_commands.h"

#define LEDC_MODE LEDC_HIGH_SPEED_MODE
static bool end_program = false;

void init_hardware() {
    init_shuffler();
    init_dispenser();
    init_turret();
    init_menu();
}

void stop(void) {
    ledc_stop(LEDC_MODE, LEDC_CHANNEL_0, 0);    
    ledc_stop(LEDC_MODE, LEDC_CHANNEL_1, 0);  
    ledc_stop(LEDC_MODE, LEDC_CHANNEL_2, 0);
    ledc_stop(LEDC_MODE, LEDC_CHANNEL_3, 0);    
}

void app_main(void)
{
    init_hardware();
    lcd_init_sequence();

    xTaskCreatePinnedToCore(turret_pid_task, "turret pid", 4096, (void *)(uintptr_t)(INITIAL_POSITION), 3, NULL, 1);
    xTaskCreatePinnedToCore(command_system_task, "command system", 4096, NULL, 2, &xSequencerTaskHandle, 1);
    xTaskCreatePinnedToCore(menu_ui_task, "menu ui task", 4096, NULL, 4, NULL, 1);

    start_button_timer();

    vTaskDelete(NULL);
}

