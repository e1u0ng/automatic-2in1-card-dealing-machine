#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

// Hardware Pin Definitions
#define GPIO_DT_PIN     GPIO_NUM_19
#define GPIO_CLK_PIN     GPIO_NUM_18
#define GPIO_MENU_SW_PIN    GPIO_NUM_5

static const char *TAG = "ENCODER";
static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

pcnt_unit_handle_t init_raw_encoder_pcnt(int gpio_a, int gpio_b) {
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_chan_config_t chan_a_config = {.edge_gpio_num = gpio_a, .level_gpio_num = gpio_b};
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    pcnt_chan_config_t chan_b_config = { .edge_gpio_num = gpio_b, .level_gpio_num = gpio_a };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    pcnt_glitch_filter_config_t filter_config = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    return pcnt_unit;
}

void init_swtich_gpio(int gpio_sw) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_sw),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio_sw, gpio_isr_handler, (void*) gpio_sw);
}

void app_main(void) {
    pcnt_unit_handle_t pcnt_unit = init_raw_encoder_pcnt(GPIO_CLK_PIN, GPIO_DT_PIN);
    init_swtich_gpio(GPIO_MENU_SW_PIN);

    int last_count = 0;
    int current_count = 0;
    uint32_t io_num;

    while (1) {
        // Read encoder position
        pcnt_unit_get_count(pcnt_unit, &current_count);
        current_count /= 4;
        if (current_count != last_count) {
            ESP_LOGI(TAG, "Encoder Position: %d", current_count);
            last_count = current_count;
        }

        // Check if button was pressed via FreeRTOS queue
        if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(10))) {
            ESP_LOGI(TAG, "Switch Pressed on GPIO %"PRIu32"!", io_num);
            // Reset counter on press if desired:
            // pcnt_unit_clear_count(pcnt_unit);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}



