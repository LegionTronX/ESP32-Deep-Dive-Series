#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Uppgift 

1. Blinka en av lamporna var annan sekund (på i 400ms av i 1600ms)
2. När vi trycker på knappen ska man byta vilken lampa som blinkar
3. Knapptryck måste märkas även om vi sover, före vi sover måste vi enable wakeup på knappen för både light och deep
4. För att spara energi, gå in i light sleep mellan blinkingarna om knappen inte tryckts på 3 sekunder

*/

// GPIO pin assignments
#define YELLOW_PIN GPIO_NUM_25
#define GREEN_PIN GPIO_NUM_32
#define BUTTON_PIN GPIO_NUM_33

// Blinking delays
#define BLINK_DELAY 400                 // Blink direction
#define SLEEP_DELAY 1600                // How long we in light sleep

// Timeout values for sleep modes
#define LIGHT_SLEEP_TIMEOUT 3000        // 3 seconds before light sleep starts
#define DEEP_SLEEP_TIMEOUT 30000        // 30 sec before deep sleep starts

// Function declarations 
void gpioConfig();
void button_interrupt_handlar(void*);
void controlLed(gpio_num_t ledPin, bool state);

const char* TAG = "MAIN";
int64_t timer_status;
int64_t timer_since_button;
int16_t delay_duration;
int64_t timer_status_current;
bool duration_toggle;
bool isPressed = false;
bool led_toggle = true;                        // false = yellow, true = green
bool button_time_run_out = false;

void app_main(void)
{
    // Configure GPIO LEDs and button
    gpioConfig();

    // Initialize timers and variables
    timer_status = esp_timer_get_time();
    timer_since_button = esp_timer_get_time();
    timer_status_current = esp_timer_get_time();
    delay_duration = BLINK_DELAY;
    duration_toggle = true;

    controlLed(GREEN_PIN, duration_toggle);           // Green led on
    

    while (1)
    {
        
        if (((timer_status_current - timer_status) > delay_duration * 1000) || isPressed)
        {
            // cheking of the leds state
            if (led_toggle)
            {
                controlLed(GREEN_PIN, duration_toggle);
                controlLed(YELLOW_PIN, false);
                ESP_LOGI(TAG, "Green LEDs are on!");
            }else
            {
                controlLed(YELLOW_PIN, duration_toggle);
                controlLed(GREEN_PIN, false);
                ESP_LOGI(TAG, "Yellow LEDs are on!");
            }

            if (!isPressed)
            {
                if (duration_toggle)
                {
                    // each time 400 ms vi are here
                    delay_duration = SLEEP_DELAY;
                    // turn off LED
                }else
                {
                    // each time 1600 ms vi are here
                    delay_duration = BLINK_DELAY;
                    // turn on LED
                }

                duration_toggle = !duration_toggle;
                timer_status = esp_timer_get_time();                // reset timer_status

            }else{
                isPressed = false;
                ESP_LOGI(TAG, "Button pressed...");
            }
           
            // if button not been pressed under 3 sec then we going in to light sleep
            if (button_time_run_out)
            { 
                ESP_LOGI(TAG, "Entering light sleep...");  
                esp_sleep_enable_ext0_wakeup(1ULL << BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);     // configure GPIO pin for wake-up
                esp_sleep_enable_timer_wakeup(delay_duration * 1000);                           // configure timer for light sleep wake-up after 1600 ms (1.6 sekunder)
                esp_light_sleep_start();                                                      // enter light sleep
            }
            
        }

        // Check if the button has not been pressed for 3 seconds, initiate light sleep
        if ((timer_status_current - timer_since_button) > LIGHT_SLEEP_TIMEOUT * 1000)
        {
            // change to light sleep mode
            button_time_run_out = true;

            if ((timer_status_current - timer_since_button) > DEEP_SLEEP_TIMEOUT * 1000)
            {
                ESP_LOGI(TAG, "Entering deep sleep... Godnight!");
                esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);     // wakeup from button
                esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);               // Turn off RTC Peripherals to avoid restart
                esp_deep_sleep_start();
            }
            
        }

        timer_status_current = esp_timer_get_time();
        
    }
}

/* Defenera functions */
void gpioConfig(){
    /* Configure button */
    esp_rom_gpio_pad_select_gpio(BUTTON_PIN);
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL<<BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&button_config);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_interrupt_handlar, (void*)BUTTON_PIN);

    /* Configure LEDs */
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL<<YELLOW_PIN | 1ULL<<GREEN_PIN),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&led_config);
}

void controlLed(gpio_num_t ledPin, bool state){
    gpio_set_level(ledPin, state);
}

void button_interrupt_handlar(void*){

    if (gpio_get_level(BUTTON_PIN) == 1)
    { 
        gpio_isr_handler_remove((gpio_num_t)BUTTON_PIN);   // switch off interrupt to avoid multiple press during debounce

        led_toggle = !led_toggle;
        isPressed = true;

        // Set a timer here, timer last time it has been pressed
        timer_since_button = esp_timer_get_time();
        button_time_run_out = false;

        // re-enable the interrupt
        gpio_isr_handler_add(BUTTON_PIN, button_interrupt_handlar, (void*)BUTTON_PIN);
    }
    
}




/*Blinka en av lamporna var annan sekund (på i 400ms av i 1600ms)​

När man trycker på knappen ska man byta vilken lampa som blinkar​

Knapptrycket måste märkas även om vi sover​

För att spara energi, gå in i light sleep mellan blinkningarna om knappen inte tryckts på på 3 sekunder​

Om inget knapptryck har skett på mer än 30 sekunder ska vi gå in i deep sleep, nu behöver vi inte blinka längre*/




