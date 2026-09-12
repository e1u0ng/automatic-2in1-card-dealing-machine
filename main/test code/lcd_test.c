#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define GPIO_SDA        GPIO_NUM_21
#define GPIO_SCL        GPIO_NUM_22
#define I2C_FREQ_HZ     100000

#define LCD_RS_COMMAND 0x00 // RS = 0
#define LCD_RS_DATA    0x01 // RS = 1
#define LCD_BACKLIGHT  0x08 // Backlight pin bit mask
#define LCD_ENABLE     0x04 // Enable bit mask

static const char *TAG = "I2C LCD";

i2c_master_dev_handle_t init_lcd(void) {
    ESP_LOGI(TAG, "Initializing I2C Master Bus...");
    
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

    ESP_LOGI(TAG, "I2C Bus Initialized successfully on SDA: GPIO %d, SCL: GPIO %d", GPIO_SDA, GPIO_SCL);

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

void app_main(void) {
    i2c_master_dev_handle_t dev = init_lcd();

    lcd_init_sequence(dev);

    lcd_send_string(dev, "SHUFFLE");

    lcd_send_command(dev, 0xC0); 
    lcd_send_string(dev, "DEAL");

    lcd_send_command(dev, 0x0F);

    /*while (1) {
        lcd_send_command(dev, 0xC0); 
        lcd_send_string(dev, "Deal");
        vTaskDelay(pdMS_TO_TICKS(250));
        lcd_send_command(dev, 0xC0); 
        lcd_send_string(dev, "                ");
        vTaskDelay(pdMS_TO_TICKS(250));
    }*/
    
}