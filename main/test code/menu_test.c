#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// Hardware Pin Definitions
#define GPIO_DT_PIN     GPIO_NUM_19
#define GPIO_CLK_PIN     GPIO_NUM_18
#define GPIO_MENU_SW_PIN    GPIO_NUM_5
#define GPIO_SHUFFLE_DEAL_SW_PIN    GPIO_NUM_25
#define POLL_INTERVAL_MS      10     // Check pin every 10ms
#define LONG_PRESS_TICKS      120    // 120 * 10ms = 1200ms (1.2s for Shuffle)
#define DEBOUNCE_TICKS        3      // 3 * 10ms = 30ms min press time

#define GPIO_SDA        GPIO_NUM_21
#define GPIO_SCL        GPIO_NUM_22
#define I2C_FREQ_HZ     100000

// LCD Commands
#define LCD_RS_COMMAND 0x00 // RS = 0
#define LCD_RS_DATA    0x01 // RS = 1
#define LCD_BACKLIGHT  0x08 // Backlight pin bit mask
#define LCD_ENABLE     0x04 // Enable bit mask

typedef struct {
    int num_players;
    int cards_per_hand;
    int extra_cards;
} game_config_t;

typedef enum {
    MENU_NAVIGATE,
    MENU_EDIT_VALUE
} menu_mode_t;

volatile bool flag_trigger_deal = false;
volatile bool flag_trigger_shuffle = false;
volatile bool flag_trigger_menu = false;

static esp_timer_handle_t button_timer;

static bool blink_state = false;
static int64_t last_blink_time = 0;

pcnt_unit_handle_t init_raw_rotary_encoder_hardware(int gpio_a, int gpio_b) {
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

static void button_timer_cb(void* arg) {
    static uint16_t shuffle_press_ticks = 0;
    static uint16_t menu_press_ticks = 0;
    static bool shuffle_handled = false;
    static bool menu_handled = false;

    bool is_shuffle_pressed = (gpio_get_level(GPIO_SHUFFLE_DEAL_SW_PIN) == 0);
    bool is_menu_pressed = (gpio_get_level(GPIO_MENU_SW_PIN) == 0);

    if (!is_shuffle_pressed) {
        if (shuffle_press_ticks >= DEBOUNCE_TICKS && !shuffle_handled) {
            flag_trigger_shuffle = true;
        }
        shuffle_press_ticks = 0;
        shuffle_handled = false;
    } else {
        shuffle_press_ticks++;
        if (shuffle_press_ticks >= LONG_PRESS_TICKS && !shuffle_handled) {
            flag_trigger_deal = true;
            shuffle_handled = true;
        }
    }

    if (!is_menu_pressed) {
        menu_press_ticks = 0;
        menu_handled = false;
    } else {
        menu_press_ticks++;
        if (menu_press_ticks >= DEBOUNCE_TICKS && !menu_handled) {
            flag_trigger_menu = true;
            menu_handled = true;
        }
    }
}

void init_swtich_gpio(void) {
    gpio_config_t io_conf_s = {
        .pin_bit_mask = (1ULL << GPIO_SHUFFLE_DEAL_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_s);

    gpio_config_t io_conf_m = {
        .pin_bit_mask = (1ULL << GPIO_MENU_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_m);

    const esp_timer_create_args_t timer_args = {
        .callback = &button_timer_cb,
        .name = "button_poller"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &button_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(button_timer, POLL_INTERVAL_MS * 1000));
}

i2c_master_dev_handle_t init_lcd(void) {
    
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

    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    return dev_handle;
}

void lcd_write_nibble(i2c_master_dev_handle_t dev, uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    
    // Pass pointers (&byte) instead of raw values
    uint8_t enable_high = data | LCD_ENABLE;
    uint8_t enable_low  = data & ~LCD_ENABLE;

    ESP_ERROR_CHECK(i2c_master_transmit(dev, &enable_high, 1, 1000));
    esp_rom_delay_us(1); // Short pulse delay for HD44780 clocking

    ESP_ERROR_CHECK(i2c_master_transmit(dev, &enable_low, 1, 1000));
    esp_rom_delay_us(50); // Latch execution delay
}

void lcd_send_command(i2c_master_dev_handle_t dev, uint8_t cmd) {
    lcd_write_nibble(dev, cmd & 0xF0, LCD_RS_COMMAND);
    lcd_write_nibble(dev, (cmd << 4) & 0xF0, LCD_RS_COMMAND);
}

void lcd_send_char(i2c_master_dev_handle_t dev, char c) {
    lcd_write_nibble(dev, c & 0xF0, LCD_RS_DATA);
    lcd_write_nibble(dev, (c << 4) & 0xF0, LCD_RS_DATA);
}

void lcd_init_sequence(i2c_master_dev_handle_t dev) {
    vTaskDelay(pdMS_TO_TICKS(50)); // Wait >15ms after power-up

    // HD44780 Standard 4-Bit Mode Reset Sequence
    lcd_write_nibble(dev, 0x30, LCD_RS_COMMAND);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_write_nibble(dev, 0x30, LCD_RS_COMMAND);
    esp_rom_delay_us(150);

    lcd_write_nibble(dev, 0x30, LCD_RS_COMMAND);
    
    // Force to 4-bit mode
    lcd_write_nibble(dev, 0x20, LCD_RS_COMMAND);

    // Standard Setup
    lcd_send_command(dev, 0x28); // 4-bit mode, 2 lines, 5x8 font
    lcd_send_command(dev, 0x0C); // Display ON, Cursor OFF, Blink OFF
    lcd_send_command(dev, 0x01); // Clear Display
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_send_command(dev, 0x06); // Entry Mode: Increment cursor
}

void lcd_send_string(i2c_master_dev_handle_t dev, const char *str) {
    while (*str) {
        lcd_send_char(dev, *str);
        str++;
    }
}

void lcd_set_cursor(i2c_master_dev_handle_t dev, uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    
    // Bounds check to prevent out-of-range memory access / commands
    if (row > 3) row = 3;
    if (col > 19) col = 19;

    // 0x80 is the base Set DDRAM Address command bit mask
    uint8_t address = 0x80 + row_offsets[row] + col;
    
    lcd_send_command(dev, address);
}

void render_menu(i2c_master_dev_handle_t dev, int selected_index, const game_config_t *config, menu_mode_t mode) {
    char line_buff[21];

    // --- LINE 0: Top Header (Machine Model & Battery) ---
    lcd_set_cursor(dev, 0, 0);

    snprintf(line_buff, sizeof(line_buff), "              [|||] ");
    lcd_send_string(dev, line_buff);

    // --- LINE 1: Values (Players, Cards/Hand, Extra Cards) ---
    lcd_set_cursor(dev, 0, 1);
    
    // Format values aligned directly over their labels
    char p_str[6], c_str[6], e_str[6];

    // Format Player Value
    if (selected_index == 0 && mode == MENU_EDIT_VALUE && blink_state) {
        snprintf(p_str, sizeof(p_str), "     ");
    } else if (selected_index == 0) {
        snprintf(p_str, sizeof(p_str), "  %2d ", config->num_players);
    } else {
        snprintf(p_str, sizeof(p_str), "  %2d ", config->num_players);
    }

    // Format Cards Per Hand Value
    if (selected_index == 1 && mode == MENU_EDIT_VALUE && blink_state) {
        snprintf(c_str, sizeof(c_str), "     ");
    } else if (selected_index == 1) {
        snprintf(c_str, sizeof(c_str), "  %2d ", config->cards_per_hand);
    } else {
        snprintf(c_str, sizeof(c_str), "  %2d ", config->cards_per_hand);
    }

    // Format Extra Cards Value
    if (selected_index == 2 && mode == MENU_EDIT_VALUE && blink_state) {
        snprintf(e_str, sizeof(e_str), "    ");
    } else if (selected_index == 2) {
        snprintf(e_str, sizeof(e_str), " %2d ", config->extra_cards);
    } else {
        snprintf(e_str, sizeof(e_str), " %2d ", config->extra_cards);
    }

    // Combine values into a centered 16-character row
    snprintf(line_buff, sizeof(line_buff), "%s  %s  %s", p_str, c_str, e_str);
    lcd_send_string(dev, line_buff);

    // --- LINE 2: Labels ---
    lcd_set_cursor(dev, 0, 2);
    snprintf(line_buff, sizeof(line_buff), "PLAYER CARD/P EXTRA");
    lcd_send_string(dev, line_buff);          
}

void app_main(void) {
    pcnt_unit_handle_t pcnt_unit = init_raw_rotary_encoder_hardware(GPIO_CLK_PIN, GPIO_DT_PIN);
    init_swtich_gpio();

    i2c_master_dev_handle_t dev = init_lcd();
    lcd_init_sequence(dev);

    game_config_t config = {
        .num_players = 4,
        .cards_per_hand = 2,
        .extra_cards = 0
    };

    menu_mode_t current_mode = MENU_NAVIGATE;

    int current_count = 0;
    int last_count = 0;
    int current_item = 0;
    bool update_display = true;

    render_menu(dev, current_item, &config, current_mode);

    while (1) {
        pcnt_unit_get_count(pcnt_unit, &current_count);
        current_count /= 4;
        if (current_count != last_count) {
            int delta = current_count - last_count;
            last_count = current_count;
            update_display = true; // Mark display for refresh

            if (current_mode == MENU_EDIT_VALUE) {
                // MODE: EDITING VALUES (Clockwise increases, Counter-Clockwise decreases)
                switch (current_item) {
                    case 0: // Players (Min 2, Max 8)
                        config.num_players += delta;
                        if (config.num_players < 2) config.num_players = 2;
                        if (config.num_players > 8) config.num_players = 8;
                        break;

                    case 1: // Cards per Hand (Min 1, Max 10)
                        config.cards_per_hand += delta;
                        if (config.cards_per_hand < 1) config.cards_per_hand = 1;
                        if (config.cards_per_hand > 10) config.cards_per_hand = 10;
                        break;

                    case 2: // Extra Cards (Min 0, Max 5)
                        config.extra_cards += delta;
                        if (config.extra_cards < 0) config.extra_cards = 0;
                        if (config.extra_cards > 5) config.extra_cards = 5;
                        break;
                }
            }
        }

        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_blink_time >= 500) { // Toggle every 500ms
            last_blink_time = now;
            blink_state = !blink_state;
            if (current_mode == MENU_EDIT_VALUE) {
                update_display = true;
            }
        }

        if (flag_trigger_menu) {
            flag_trigger_menu = false;
            if (current_mode == MENU_NAVIGATE) {
                current_mode = MENU_EDIT_VALUE;
                current_item = 0;
            } else {
                switch(current_item) {
                    case 0:
                        current_item = 1;
                        break;
                    case 1:
                        current_item = 2;
                        break;
                    case 2:
                        current_item = 0;
                        current_mode = MENU_NAVIGATE;
                        break;        
                }
            }
            //current_mode = (current_mode == MENU_NAVIGATE) ? MENU_EDIT_VALUE : MENU_NAVIGATE;
            update_display = true;
        }
        if (flag_trigger_shuffle) {
            flag_trigger_shuffle = false;
            printf("SHUFFLING...\n");
        }
        if (flag_trigger_deal) {
            flag_trigger_deal = false;
            printf("DEALING...\n");
        }
        if (update_display) {
            render_menu(dev, current_item, &config, current_mode);
            update_display = false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}