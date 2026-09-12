#include <stdio.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <driver/pulse_cnt.h>
#include <driver/gpio.h>
#include <esp_timer.h>

#include "menu.h"
#include "system_commands.h"

#define GPIO_MAIN_CONTROl_DT_PIN            GPIO_NUM_27
#define GPIO_MAIN_CONTROl_CLK_PIN           GPIO_NUM_14
#define GPIO_MAIN_CONTROl_MENU_SW_PIN       GPIO_NUM_26
#define GPIO_SHUFFLE_DEAL_DT_PIN            GPIO_NUM_13
#define GPIO_SHUFFLE_DEAL_CLK_PIN           GPIO_NUM_23
#define GPIO_SHUFFLE_DEAL_SW_PIN            GPIO_NUM_25
#define GPIO_SDA                            GPIO_NUM_21
#define GPIO_SCL                            GPIO_NUM_22

#define POLL_INTERVAL_MS      10     // Check pin every 10ms
#define DEBOUNCE_TICKS        3      // 3 * 10ms = 30ms min press time
#define I2C_FREQ_HZ           100000

#define LCD_RS_COMMAND 0x00 // RS = 0
#define LCD_RS_DATA    0x01 // RS = 1
#define LCD_BACKLIGHT  0x08 // Backlight pin bit mask
#define LCD_ENABLE     0x04 // Enable bit mask

game_config_t config = {
        .num_players = 4,
        .cards_per_hand = 2,
        .extra_cards = 0
    };

menu_mode_t menu_mode = MENU_NAVIGATE;
config_selection_t config_selection = NONE;
shuffle_deal_mode_t shuffle_deal_mode = SHUFFLE;

static pcnt_unit_handle_t menu_pcnt_unit = NULL;
static pcnt_unit_handle_t shuffle_deal_pcnt_unit = NULL;
static i2c_master_dev_handle_t dev = NULL;
static esp_timer_handle_t button_timer;

int64_t last_blink_time = 0;
bool g_menu_button_pressed = false;
bool blink_state = false;

void init_raw_rotary_encoder_hardware(pcnt_unit_handle_t *unit, int gpio_a, int gpio_b) {
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, unit));

    pcnt_chan_config_t chan_a_config = {.edge_gpio_num = gpio_a, .level_gpio_num = gpio_b};
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(*unit, &chan_a_config, &pcnt_chan_a));

    pcnt_chan_config_t chan_b_config = { .edge_gpio_num = gpio_b, .level_gpio_num = gpio_a };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(*unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    pcnt_glitch_filter_config_t filter_config = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(*unit, &filter_config));

    ESP_ERROR_CHECK(pcnt_unit_enable(*unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(*unit));
    ESP_ERROR_CHECK(pcnt_unit_start(*unit));
}

void update_menu_and_config_states(void);
void render_menu(void);

static void button_timer_cb(void* arg) {
    static uint16_t shuffle_press_ticks = 0;
    static uint16_t menu_press_ticks = 0;
    static bool shuffle_handled = false;
    static bool menu_handled = false;

    bool is_shuffle_pressed = (gpio_get_level(GPIO_SHUFFLE_DEAL_SW_PIN) == 0);
    bool is_menu_pressed = (gpio_get_level(GPIO_MAIN_CONTROl_MENU_SW_PIN) == 0);

    if (xSequencerTaskHandle != NULL) {
        switch(shuffle_deal_mode) {
            case SHUFFLE:
                static sequencer_cmd_t last_cmd = CMD_STOP_SHUFFLE;
                sequencer_cmd_t target_cmd = is_shuffle_pressed ? CMD_START_SHUFFLE : CMD_STOP_SHUFFLE;

                if (target_cmd != last_cmd) {
                    xTaskNotify(xSequencerTaskHandle, target_cmd, eSetValueWithOverwrite);
                    last_cmd = target_cmd;
                }
                break;
            case DEAL:
                if (!is_shuffle_pressed) {
                    shuffle_press_ticks = 0;
                    shuffle_handled = false;     
                    break;           
                }
                shuffle_press_ticks++;
                if (shuffle_press_ticks > DEBOUNCE_TICKS && !shuffle_handled) {
                    xTaskNotify(xSequencerTaskHandle, CMD_START_DEAL, eSetValueWithOverwrite);
                    shuffle_handled = true;
                }
                break;
            case SHUFFLE_DEAL:
                if (!is_shuffle_pressed) {
                    shuffle_press_ticks = 0;
                    shuffle_handled = false;
                    break;                
                }
                shuffle_press_ticks++;
                if (shuffle_press_ticks > DEBOUNCE_TICKS && !shuffle_handled) {
                    xTaskNotify(xSequencerTaskHandle, CMD_START_SHUFFLE_AND_DEAL, eSetValueWithOverwrite);
                    shuffle_handled = true;
                } 
                break;    
        }
    }

    if (!is_menu_pressed) {
        menu_press_ticks = 0;
        menu_handled = false;
    } else {
        menu_press_ticks++;
        if (menu_press_ticks >= DEBOUNCE_TICKS && !menu_handled) {
            menu_handled = true;
            g_menu_button_pressed = true;
        }
    }
}

void init_switch_gpio(void) {
    gpio_config_t io_conf_s = {
        .pin_bit_mask = (1ULL << GPIO_SHUFFLE_DEAL_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_s);

    gpio_config_t io_conf_m = {
        .pin_bit_mask = (1ULL << GPIO_MAIN_CONTROl_MENU_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_m);
}

void start_button_timer(void) {
    const esp_timer_create_args_t timer_args = {
        .callback = &button_timer_cb,
        .name = "button_poller"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &button_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(button_timer, POLL_INTERVAL_MS * 1000));
}

void init_lcd(void) {
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_SDA,
        .scl_io_num = GPIO_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x27, // LCD or target device I2C address
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev));
}

void init_menu(void) {
    init_raw_rotary_encoder_hardware(&menu_pcnt_unit,GPIO_MAIN_CONTROl_CLK_PIN,GPIO_MAIN_CONTROl_DT_PIN);
    init_raw_rotary_encoder_hardware(&shuffle_deal_pcnt_unit,GPIO_SHUFFLE_DEAL_CLK_PIN,GPIO_SHUFFLE_DEAL_DT_PIN);
    init_switch_gpio();
    init_lcd();
}

void lcd_write_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    
    // Pass pointers (&byte) instead of raw values
    uint8_t enable_high = data | LCD_ENABLE;
    uint8_t enable_low  = data & ~LCD_ENABLE;

    ESP_ERROR_CHECK(i2c_master_transmit(dev, &enable_high, 1, 1000));
    esp_rom_delay_us(1); // Short pulse delay for HD44780 clocking

    ESP_ERROR_CHECK(i2c_master_transmit(dev, &enable_low, 1, 1000));
    esp_rom_delay_us(50); // Latch execution delay
}

void lcd_send_command(uint8_t cmd) {
    lcd_write_nibble(cmd & 0xF0, LCD_RS_COMMAND);
    lcd_write_nibble((cmd << 4) & 0xF0, LCD_RS_COMMAND);
}

void lcd_send_char(char c) {
    lcd_write_nibble(c & 0xF0, LCD_RS_DATA);
    lcd_write_nibble((c << 4) & 0xF0, LCD_RS_DATA);
}

void lcd_init_sequence(void) {
    vTaskDelay(pdMS_TO_TICKS(50)); // Wait >15ms after power-up

    // HD44780 Standard 4-Bit Mode Reset Sequence
    lcd_write_nibble(0x30, LCD_RS_COMMAND);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_write_nibble(0x30, LCD_RS_COMMAND);
    esp_rom_delay_us(150);

    lcd_write_nibble(0x30, LCD_RS_COMMAND);
    
    // Force to 4-bit mode
    lcd_write_nibble(0x20, LCD_RS_COMMAND);

    // Standard Setup
    lcd_send_command(0x28); // 4-bit mode, 2 lines, 5x8 font
    lcd_send_command(0x0C); // Display ON, Cursor OFF, Blink OFF
    lcd_send_command(0x01); // Clear Display
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_send_command(0x06); // Entry Mode: Increment cursor

    render_menu();
}

void lcd_send_string(const char *str) {
    while (*str) {
        lcd_send_char(*str);
        str++;
    }
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    
    // Bounds check to prevent out-of-range memory access / commands
    if (row > 3) row = 3;
    if (col > 19) col = 19;

    // 0x80 is the base Set DDRAM Address command bit mask
    uint8_t address = 0x80 + row_offsets[row] + col;
    
    lcd_send_command(address);
}

int get_num_player(void) {
    return config.num_players;
}

int get_cards_per_hand(void) {
    return config.cards_per_hand;
}

int get_extra_cards(void) {
    return config.extra_cards;
}

void set_num_player(int num) {
    if (num < 2) num = 2;
    if (num > 8) num = 8;
    config.num_players = num;
}

void set_cards_per_hand(int num) {
    if (num < 1) num = 1;
    if (num > 10) num = 10;
    config.cards_per_hand = num;
}

void set_extra_cards(int num) {
    if (num < 0) num = 0;
    if (num > 5) num = 5;
    config.extra_cards = num;
}

void blinking_value_timer(int ms) {
    int64_t now = esp_timer_get_time() / 1000;
    if (now - last_blink_time >= ms) {
        last_blink_time = now;
        blink_state = !blink_state;
        if (menu_mode == MENU_EDIT_VALUE) render_menu();
    }
}

void render_menu(void) {
    char line_buff[21];

    // --- LINE 0: Top Header (Machine Model & Battery) ---
    lcd_set_cursor(0, 0);

    char mode[3];

    switch(shuffle_deal_mode) {
        case SHUFFLE:
            snprintf(mode, sizeof(mode), " S");
            break;
        case DEAL:
            snprintf(mode, sizeof(mode), " D");
            break;
        case SHUFFLE_DEAL:
            snprintf(mode, sizeof(mode), "SD");
            break;
    }

    snprintf(line_buff, sizeof(line_buff), "              [%s] ", mode);
    
    lcd_send_string(line_buff);

    // --- LINE 1: Values (Players, Cards/Hand, Extra Cards) ---
    lcd_set_cursor(0, 1);
    
    // Format values aligned directly over their labels
    char p_str[6], c_str[6], e_str[6];

    // Format Player Value
    if (config_selection == PLAYER && menu_mode == MENU_EDIT_VALUE && blink_state) {
        snprintf(p_str, sizeof(p_str), "     ");
    } else if (config_selection == PLAYER) {
        snprintf(p_str, sizeof(p_str), "  %2d ", config.num_players);
    } else {
        snprintf(p_str, sizeof(p_str), "  %2d ", config.num_players);
    }

    // Format Cards Per Hand Value
    if (config_selection == CARDS && menu_mode == MENU_EDIT_VALUE && blink_state) {
        snprintf(c_str, sizeof(c_str), "     ");
    } else if (config_selection == CARDS) {
        snprintf(c_str, sizeof(c_str), "  %2d ", config.cards_per_hand);
    } else {
        snprintf(c_str, sizeof(c_str), "  %2d ", config.cards_per_hand);
    }

    // Format Extra Cards Value
    if (config_selection == EXTRA && menu_mode == MENU_EDIT_VALUE && blink_state) {
        snprintf(e_str, sizeof(e_str), "    ");
    } else if (config_selection == EXTRA) {
        snprintf(e_str, sizeof(e_str), " %2d ", config.extra_cards);
    } else {
        snprintf(e_str, sizeof(e_str), " %2d ", config.extra_cards);
    }

    // Combine values into a centered 16-character row
    snprintf(line_buff, sizeof(line_buff), "%s  %s  %s", p_str, c_str, e_str);
    lcd_send_string(line_buff);

    // --- LINE 2: Labels ---
    lcd_set_cursor(0, 2);
    snprintf(line_buff, sizeof(line_buff), "PLAYER CARD/P EXTRA");
    lcd_send_string(line_buff);    
}

/**
 * @brief Returns the number of detent steps moved since last check.
 * @return +1 for clockwise click, -1 for counter-clockwise click, 0 if unmoved.
 */
int get_menu_encoder_delta(void) {
    if (menu_pcnt_unit == NULL) {
        ESP_LOGE("ENCODER", "menu_pcnt_unit is NULL! PCNT driver was not initialized.");
        return 0;
    }

    int current_count = 0;
    pcnt_unit_get_count(menu_pcnt_unit, &current_count);

    // Calculate detent clicks (4 pulses per physical click)
    int steps = current_count / 4; 

    if (steps != 0) {
        // Reset hardware PCNT counter back to 0 so we don't overflow
        pcnt_unit_clear_count(menu_pcnt_unit); 
    }

    return steps;
}

/**
 * @brief Returns the number of detent steps moved since last check.
 * @return +1 for clockwise click, -1 for counter-clockwise click, 0 if unmoved.
 */
int get_shuffle_encoder_delta(void) {
    if (menu_pcnt_unit == NULL) {
        ESP_LOGE("ENCODER", "menu_pcnt_unit is NULL! PCNT driver was not initialized.");
        return 0;
    }

    int current_count = 0;
    pcnt_unit_get_count(shuffle_deal_pcnt_unit, &current_count);

    // Calculate detent clicks (4 pulses per physical click)
    int steps = current_count / 4; 

    if (steps != 0) {
        // Reset hardware PCNT counter back to 0 so we don't overflow
        pcnt_unit_clear_count(shuffle_deal_pcnt_unit); 
    }

    return steps;
}

void update_config_values(void) {
    int delta = get_menu_encoder_delta();
    if (delta == 0 || menu_mode != MENU_EDIT_VALUE) return;
    
    if (config_selection == NONE) config_selection = PLAYER;
    switch (config_selection) {
        case PLAYER: // Players (Min 2, Max 8)
            set_num_player(get_num_player() + delta);
            break;
        case CARDS: // Cards per Hand (Min 1, Max 10)
            set_cards_per_hand(get_cards_per_hand() + delta);
            break;
        case EXTRA: // Extra Cards (Min 0, Max 5)
            set_extra_cards(get_extra_cards() + delta);
            break;
        case NONE:
            break;    
    }
    render_menu();
}

void update_shuffle_deal_mode(void) {
    int delta = get_shuffle_encoder_delta();
    if (delta == 0) return;

    int index = ((int)shuffle_deal_mode + delta) % 3;

    if (index < 0) index += 3;

    shuffle_deal_mode = (shuffle_deal_mode_t)index;
    render_menu();
}

void update_menu_and_config_states(void) {
    if (menu_mode == MENU_NAVIGATE) {
        menu_mode = MENU_EDIT_VALUE;
        config_selection = PLAYER;
    } else {
        switch(config_selection) {
            case PLAYER:
                config_selection = CARDS;
                break;
            case CARDS:
                config_selection = EXTRA;
                break;
            case EXTRA:
                config_selection = NONE;
                menu_mode = MENU_NAVIGATE;
                break;
            case NONE:
                break;            
        }
    }
    render_menu();
}

void menu_ui_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        if (g_menu_button_pressed) {
            g_menu_button_pressed = false;
            update_menu_and_config_states();
        }

        update_config_values();
        update_shuffle_deal_mode();
        blinking_value_timer(500);

        //ESP_LOGI("STATE", "Current State Int: %d", (int)shuffle_deal_mode);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}


