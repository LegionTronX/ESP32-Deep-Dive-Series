#include <stdio.h>  // Standard input/output header for handling input and output.
#include "freertos/FreeRTOS.h"  // Includes the FreeRTOS library for multitasking and real-time capabilities.
#include "freertos/task.h"  // Provides access to task functionalities in FreeRTOS.
#include "driver/gpio.h"  // GPIO driver for handling general-purpose input/output pins.
#include "driver/i2c.h"  // I2C driver for handling I2C communication.
#include "driver/adc.h"  // ADC driver for handling analog-to-digital conversion.
#include "driver/ledc.h"  // LEDC driver for handling PWM operations.
#include "esp_adc_cal.h"  // Provides functionalities for ADC calibration.
#include "esp_log.h"  // Logging library to output debugging information.
// Define GPIO pins and I2C address for I2C communication
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_SLAVE_SCL_IO 23
#define I2C_SLAVE_SDA_IO 19
#define I2C_SLAVE_ADDR 0X28

// Define GPIO pin for button and ADC channel
#define BUTTON_GPIO GPIO_NUM_33
#define ADC_CHANNEL ADC1_CHANNEL_6

// Define GPIO pin for servo control and LEDC configurations
#define SERVO_PIN GPIO_NUM_32
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_FREQUENCY 50
#define LEDC_RESOLUTION LEDC_TIMER_12_BIT

// Declare variables for ADC operation and data handling
static esp_adc_cal_characteristics_t *adc_chars;
static volatile bool has_new_adc_value = false;
static volatile uint32_t adc_value = 0;

// ISR (Interrupt Service Routine) triggered by button press to update ADC value
static void IRAM_ATTR button_isr_handler(void* arg) {
    adc_value = adc1_get_raw(ADC_CHANNEL);
    has_new_adc_value = true;
}

// Task executed by the I2C master
static void i2c_master_task(void *arg) {
    // Configure I2C parameters for master mode
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, i2c_config.mode, 0, 0, 0));

    uint8_t data[2];
    while (1) {
        // Check if there's a new ADC value
        if (has_new_adc_value) {
            // Prepare data to be sent via I2C
            data[0] = adc_value >> 8;
            data[1] = adc_value & 0xFF;
            // Write data to I2C slave device
            ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_NUM_0, I2C_SLAVE_ADDR, data, sizeof(data), 1000 / portTICK_PERIOD_MS));
            // Reset flag after sending data
            has_new_adc_value = false;
        }
        // Delay to stabilize readings
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task executed by the I2C slave
static void i2c_slave_task(void *arg) {
    // Configure I2C parameters for slave mode
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .slave.addr_10bit_en = 0,
        .slave.slave_addr = I2C_SLAVE_ADDR
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_1, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_1, i2c_config.mode, 512, 512, 0));

    uint8_t data[2];
    while (1) {
        // Read data from I2C master
        int len = i2c_slave_read_buffer(I2C_NUM_1, data, sizeof(data), portMAX_DELAY);
        if (len == 2) {
            // Process ADC reading received from master and update LEDC
            uint32_t adc_reading = (data[0] << 8) | data[1];
            // Convert ADC reading to PWM duty cycle and update LEDC
            // Note: Actual implementation depends on your application
        }
    }
}

void app_main(void) {
    // Initialize ADC
    adc1_config_width(ADC_WIDTH_BIT_12);  // Set ADC width to 12 bits
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);  // Configure ADC attenuation for the specified channel
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));  // Allocate memory for ADC characteristics
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);  // Initialize ADC calibration data

    // Initialize button interrupt
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);  // Set button GPIO pin as input
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);  // Set pull-up resistor mode for button GPIO pin
    gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_POSEDGE);  // Configure button interrupt type as positive edge
    gpio_install_isr_service(0);  // Install GPIO ISR service
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);  // Add button ISR handler

    // Initialize LEDC for servo control
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,  // Set LEDC speed mode
        .timer_num = LEDC_TIMER,  // Set LEDC timer number
        .freq_hz = LEDC_FREQUENCY,  // Set PWM frequency
        .duty_resolution = LEDC_RESOLUTION,  // Set PWM duty resolution
        .clk_cfg = LEDC_AUTO_CLK,  // Use auto clock
    };
    ledc_timer_config(&ledc_timer);  // Configure LEDC timer

    ledc_channel_config_t ledc_channel = {
        .gpio_num = SERVO_PIN,  // Set GPIO pin for servo control
        .speed_mode = LEDC_MODE,  // Set LEDC speed mode
        .channel = LEDC_CHANNEL,  // Set LEDC channel
        .timer_sel = LEDC_TIMER,  // Set LEDC timer
        .duty = 0,  // Initialize PWM duty cycle to 0
        .hpoint = 0,  // Initialize PWM hpoint to 0
        .intr_type = LEDC_INTR_DISABLE,  // Disable LEDC interrupt
    };
    ledc_channel_config(&ledc_channel);  // Configure LEDC channel

    // Start I2C tasks
    xTaskCreate(i2c_master_task, "i2c_master_task", 2048, NULL, 10, NULL);  // Create I2C master task
    xTaskCreate(i2c_slave_task, "i2c_slave_task", 2048, NULL, 11, NULL);  // Create I2C slave task
}


// Essential Tips for I2C Communication
// 1. Address Uniqueness: Ensure each I2C device on the bus has a unique address to avoid conflicts.
// 2. Pull-up Resistors: Properly configure pull-up resistors on SDA and SCL lines for signal stability.
// 3. Error Handling: Implement robust error checking to handle communication failures effectively.
// 4. Clock Speed: Optimize clock speed for reliable communication, balancing speed and signal integrity.
// 5. Buffer Management: Manage data buffers efficiently to prevent overflow or underflow.
// 6. Testing: Thoroughly test communication under various conditions for reliability.
// 7. Noise Reduction: Minimize noise interference by keeping bus lines short and isolated from noise sources.
// 8. Documentation: Document device addresses and communication protocols for future reference.
// 9. Resource Management: Manage resources effectively to avoid contention and ensure smooth operation.
// 10. Clock Stretching: Check compatibility with clock stretching if needed for specific devices.

// Note: Refer to ESP-IDF documentation for detailed I2C configuration and troubleshooting:
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
