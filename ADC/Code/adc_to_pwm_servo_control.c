#include <stdio.h>  // Standard input/output header for handling input and output.
#include "freertos/FreeRTOS.h"  // Includes the FreeRTOS library for multitasking and real-time capabilities.
#include "freertos/task.h"  // Provides access to task functionalities in FreeRTOS.
#include "driver/gpio.h"  // GPIO driver for handling general-purpose input/output pins.
#include "driver/adc.h"  // ADC driver for handling analog-to-digital conversion.
#include "driver/ledc.h"  // LEDC driver for handling PWM operations.
#include "esp_adc_cal.h"  // Provides functionalities for ADC calibration.
#include "esp_log.h"  // Logging library to output debugging information.

// Constants defining the maximum values for ADC and PWM resolutions and pulse calculations
#define MAX_ADC_VALUE 4095  // Maximum value for 12-bit ADC resolution.
#define MAX_PWM_VALUE 4095  // Assumes the highest resolution for your PWM configuration.
#define PWM_1_MS_VALUE (MAX_PWM_VALUE / 20)  // Defines a 1 ms pulse width based on PWM configuration.
#define PWM_2_MS_VALUE ((MAX_PWM_VALUE / 10) + 100)  // Defines a 2 ms pulse width.

#define POTENTIOMETER_ADC_CHANNEL ADC1_CHANNEL_6  // Assigns the potentiometer to ADC channel 6.
#define SERVO_PIN GPIO_NUM_32  // Defines the GPIO pin connected to the servo.

static const char* TAG = "app_main";  // Tag used for logging messages.
static esp_adc_cal_characteristics_t *adc_chars;  // Pointer to store ADC characteristics.

void app_main(void) {
    // Set ADC width to 12 bits and configure the attenuation for channel 6.
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(POTENTIOMETER_ADC_CHANNEL, ADC_ATTEN_DB_11);
    
    // Allocate memory and initialize ADC calibration data.
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);

    // Set up the PWM timer configuration with a 50 Hz frequency and 12-bit resolution.
    ledc_timer_config_t pwm_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&pwm_timer));

    // Configure the PWM channel tied to the defined servo pin.
    ledc_channel_config_t pwm_channel = {
        .gpio_num = SERVO_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,  // Start with a duty cycle of 0 to prevent unwanted movement.
        .hpoint = 0,
        .intr_type = LEDC_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&pwm_channel));

    // Continuously read the potentiometer value and adjust the servo position.
    while (true) {
        uint32_t pot_value = adc1_get_raw(POTENTIOMETER_ADC_CHANNEL);
        uint32_t pwm_value = ((pot_value * (PWM_2_MS_VALUE - PWM_1_MS_VALUE)) / MAX_ADC_VALUE) + PWM_1_MS_VALUE;

        // Update the PWM duty cycle with the calculated value.
        ledc_set_duty(pwm_channel.speed_mode, pwm_channel.channel, pwm_value);
        ledc_update_duty(pwm_channel.speed_mode, pwm_channel.channel);

        // Log the current potentiometer and PWM values.
        ESP_LOGI(TAG, "Potentiometer Value: %lu, PWM Value: %lu", pot_value, pwm_value);
        vTaskDelay(pdMS_TO_TICKS(100));  // Delay to stabilize readings.
    }
}
