#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"

#define INTERRUPT_PIN GPIO_NUM_33 // GPIO 33 for the button
#define LED_PIN GPIO_NUM_32       // GPIO 32 for the LED

static uint32_t i = 0; // Initialize a counter variable

volatile bool buttonPressed = false; // Declare a volatile flag variable to track button press

// Interrupt handler for GPIO button press
void IRAM_ATTR gpio_interrupt_handler(void *arg) {
    int currentLevel = gpio_get_level(LED_PIN); // Get the current level of the LED pin
    gpio_set_level(LED_PIN, !currentLevel); // Toggle LED state
    i += 10; // Simulate an action by incrementing the counter
    buttonPressed = true; // Set the button press flag
    
    // Optionally, signal the main loop to reset the WDT or perform direct WDT reset if applicable
}

void app_main(void) {
    // Configure WDT
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = 10000, // Timeout after 10 seconds
        .trigger_panic = false, // Do not trigger panic on WDT timeout
        .idle_core_mask = NULL // Use all cores for WDT
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdtConfig)); // Reconfigure the WDT with the specified parameters
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); // Add the current task to the WDT watchlist

    // Configure GPIO (button and LED as before)
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << INTERRUPT_PIN), // Set the pin bitmask for the interrupt pin
        .mode = GPIO_MODE_INPUT, // Configure as input mode
        .intr_type = GPIO_INTR_HIGH_LEVEL // Interrupt type for rising edge
    };
    gpio_config(&io_config); // Configure GPIO with the specified parameters
    gpio_install_isr_service(0); // Install the GPIO ISR service
    gpio_isr_handler_add(INTERRUPT_PIN, gpio_interrupt_handler, (void*)INTERRUPT_PIN); // Add interrupt handler for the button
    esp_rom_gpio_pad_select_gpio(LED_PIN); // Select the GPIO pad for the LED pin
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT); // Set LED pin direction as output
    gpio_set_level(LED_PIN, 0); // Assume 0 turns off the LED initially

    // Main loop
    for (;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait for 1 second
        ESP_LOGI("main", "i = %lu", i++); // Log the value of 'i'
        if (buttonPressed) {
            esp_task_wdt_reset(); // Reset the WDT to prevent timeout
            buttonPressed = false; // Reset the button press flag
        }
    }
}



// Essential Tips for Watchdog Implementation
// 1. Timeout Period: Choose an appropriate timeout period based on the system's expected behavior and response time requirements.
// 2. Task Reconfiguration: Reconfigure the watchdog timer if task execution times change significantly to prevent premature resets or timeouts.
// 3. Error Handling: Implement error handling mechanisms to detect and recover from unexpected errors or faults that may trigger the watchdog timer.
// 4. Task Prioritization: Prioritize critical tasks to ensure they complete within the watchdog timeout period, minimizing the risk of system resets.
// 5. Reset Frequency: Reset the watchdog timer at regular intervals to prevent it from reaching its timeout and triggering a system reset.
// 6. Testing: Thoroughly test the watchdog functionality under various conditions to validate its effectiveness in preventing system crashes.
// 7. System Health Monitoring: Monitor system health indicators and log watchdog resets to identify potential issues and improve system reliability.
// 8. Interrupt Handling: Implement interrupt handlers to respond promptly to critical events and reset the watchdog timer as needed.
// 9. Documentation: Document the watchdog configuration settings and reset logic for future reference and troubleshooting.
// 10. System Recovery: Implement recovery mechanisms to gracefully handle watchdog timeouts and restore the system to a stable state.

// Note: Refer to ESP-IDF documentation for detailed watchdog configuration and troubleshooting:
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/wdts.html

//Interrupt

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
