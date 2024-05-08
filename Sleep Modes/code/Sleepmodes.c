#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/rtc_io.h"

#define BLUE_PIN GPIO_NUM_25
#define RED_PIN GPIO_NUM_32
#define BUTTON_PIN GPIO_NUM_26

#define LED_ON_TIME_MS 400
#define LED_OFF_TIME_MS 1600
#define LIGHT_SLEEP_THRESHOLD_US 3000000 // 3 seconds in microseconds
#define DEEP_SLEEP_THRESHOLD_US 30000000 // 30 seconds in microseconds

const char* TAG = "MAIN";

RTC_DATA_ATTR static bool chooseLED = false;
RTC_DATA_ATTR static int64_t lastButtonPressTime = 0;

void lampColours(bool chooseLED) {
    gpio_set_level(BLUE_PIN, chooseLED ? 0 : 1);
    gpio_set_level(RED_PIN, chooseLED ? 1 : 0);
}

void button_interrupt_handler(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    if (gpio_num == BUTTON_PIN) {
        chooseLED = !chooseLED;
        lampColours(chooseLED);
        lastButtonPressTime = esp_timer_get_time(); // Update time on button press
        ESP_LOGI(TAG, "Button pressed: Switching LED to %s", chooseLED ? "Red" : "Blue");
    }
}

void app_main(void) {
    gpio_set_direction(BLUE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RED_PIN, GPIO_MODE_OUTPUT);

    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0); // Wake up on low level from BUTTON_PIN

    gpio_pad_select_gpio(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(BUTTON_PIN, button_interrupt_handler, (void*) BUTTON_PIN);

    lastButtonPressTime = esp_timer_get_time();

    while (true) {
        int64_t currentTime = esp_timer_get_time();
        
        lampColours(chooseLED); // Control LEDs based on current state
        vTaskDelay(pdMS_TO_TICKS(LED_ON_TIME_MS));
        gpio_set_level(BLUE_PIN, 0);
        gpio_set_level(RED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(LED_OFF_TIME_MS));

        currentTime = esp_timer_get_time(); // Update the current time before checking sleep conditions

        // Enter light sleep if no button press for over 3 seconds
        if (currentTime - lastButtonPressTime > LIGHT_SLEEP_THRESHOLD_US) {
            ESP_LOGI(TAG, "Entering light sleep due to inactivity");
            esp_sleep_enable_timer_wakeup(5000 * 1000); // Wake up after 5 seconds if no other wake-up triggers
            esp_light_sleep_start();
            ESP_LOGI(TAG, "Woke up from light sleep");
            lastButtonPressTime = esp_timer_get_time(); // Refresh the timer as waking up resets the MCU
        }

        // Enter deep sleep if no button press for over 30 seconds
        if (currentTime - lastButtonPressTime > DEEP_SLEEP_THRESHOLD_US) {
            ESP_LOGI(TAG, "No activity for 30 seconds, entering deep sleep");
            gpio_set_level(BLUE_PIN, 0);
            gpio_set_level(RED_PIN, 0); // Ensure LEDs are turned off before deep sleep
            esp_deep_sleep_start();
        }
    }
}


