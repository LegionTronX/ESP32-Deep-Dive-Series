#include <stdio.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_main.h"
#include "sdkconfig.h"
#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "freertos/queue.h"  // Include FreeRTOS queue support

// BLE characteristics UUIDs
#define POTENTIOMETER_CHAR_UUID 0xAA03
#define LED_CHAR_UUID           0xAA04

// Hardware configuration
#define POTENTIOMETER_ADC_CHANNEL ADC1_CHANNEL_6
#define LED_GPIO                 GPIO_NUM_2

// Device configuration
#define DEVICE_NAME      "BLE ITHS"
#define GATTS_NUM_HANDLE_A 4

// BLE service setup
#define SERVICE_A            0
#define SERVICE_A_UUID       0xAA01
#define SERVICE_A_CHAR_1_UUID 0xAA02
#define SERVICE_B            1

// Advertising flags
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

// Global variables
static esp_adc_cal_characteristics_t *adc_chars;
static uint16_t potentiometer_char_handle;
QueueHandle_t led_queue;
volatile bool led_state = false;

 
static uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};
 
static uint8_t adv_config_done = 0;
 
static esp_ble_adv_params_t adv_param = {
  .adv_int_min = 0x20,
  .adv_int_max = 0x40,
  .adv_type = ADV_TYPE_IND,
  .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .channel_map = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
 
struct gatts_service{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
    uint16_t led_char_handle;
};
 
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
 
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
 
static uint8_t service_a_char_1[] = {0xAA, 0x01, 0x01, 0x01};
static esp_attr_value_t service_a_char_1_val = {
  .attr_max_len = 4,
  .attr_len = 4,
  .attr_value = service_a_char_1, 
};
 
static void service_a_event_handler (esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void service_b_event_handler (esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static struct gatts_service service_tab[2] = {
  [SERVICE_A] = {
    .gatts_cb = service_a_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,
  },
  [SERVICE_B] = {
    .gatts_cb = service_b_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,
  }
};

void adc_init() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(POTENTIOMETER_ADC_CHANNEL, ADC_ATTEN_DB_11);

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);
}

void update_ble_potentiometer_value(uint32_t adc_value) {
    uint8_t value[2];  // Example: we are just splitting the uint32 into two bytes
    value[0] = (adc_value >> 8) & 0xFF; // High byte
    value[1] = adc_value & 0xFF;        // Low byte

    // Assuming `potentiometer_char_handle` is the handle for the BLE characteristic
    esp_err_t ret = esp_ble_gatts_set_attr_value(potentiometer_char_handle, sizeof(value), value);
    if (ret != ESP_OK) {
        ESP_LOGE("update_value", "Error updating potentiometer value: %s", esp_err_to_name(ret));
    }
}

void potentiometer_task(void *pvParameter) {
    uint32_t adc_value;
    while (1) {
        adc_value = adc1_get_raw(POTENTIOMETER_ADC_CHANNEL);
        // Optionally convert ADC value using esp_adc_cal_raw_to_voltage if needed
        // int millivolts = esp_adc_cal_raw_to_voltage(adc_value, adc_chars);

        update_ble_potentiometer_value(adc_value);  // This needs to be defined to update BLE characteristic
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
    }
}

void led_init() {
    esp_rom_gpio_pad_select_gpio(LED_GPIO);  // Uppdaterad för att använda korrekt ESP-IDF funktion
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    led_queue = xQueueCreate(10, sizeof(bool));  // Skapar en kö för att hantera bool-värden
}


void set_led_state(bool on) {
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

void led_control_task(void *pvParameter) {
    bool state;
    while (1) {
        if (xQueueReceive(led_queue, &state, portMAX_DELAY)) {
            gpio_set_level(LED_GPIO, state);
        }
    }
}




 
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
 
void app_main(void)
{
  esp_err_t err;
 
  err = nvs_flash_init();
  if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
 
  esp_bt_controller_config_t bt_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&bt_config));
 
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
 
  ESP_ERROR_CHECK(esp_bluedroid_init());
 
  ESP_ERROR_CHECK(esp_bluedroid_enable());
 
  ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
  ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
 
  ESP_ERROR_CHECK(esp_ble_gatts_app_register(SERVICE_A));
 
  ESP_ERROR_CHECK(esp_ble_gatts_app_register(SERVICE_B));
 
  ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(512));

  adc_init();

  xTaskCreate(potentiometer_task, "PotentiometerTask", 2048, NULL, 10, NULL);

  led_init();
    xTaskCreate(led_control_task, "LED Control Task", 2048, NULL, 10, NULL);



  for (;;){
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
 
 
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){
  if(event == ESP_GATTS_REG_EVT){
    if(param->reg.status == ESP_GATT_OK){
      service_tab[param->reg.app_id].gatts_if = gatts_if;
    } else {
      ESP_LOGI("gatts_event", "Reg app failed");
      return;
    }
  }
 
  do {
    int idx;
    for(idx = 0; idx < 2; idx++){
      if(gatts_if == ESP_GATT_IF_NONE || gatts_if == service_tab[idx].gatts_if){
        if(service_tab[idx].gatts_cb){
          service_tab[idx].gatts_cb(event, gatts_if, param);
        }
      } 
    }
  }while (0);
}
 
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param){
  switch (event)
  {
  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      adv_config_done &= (~adv_config_flag);
      if(adv_config_done == 0){
        esp_ble_gap_start_advertising(&adv_param);
      }
    break;
  case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
    adv_config_done &= (~scan_rsp_config_flag);
    if(adv_config_done == 0){
      esp_ble_gap_start_advertising(&adv_param);
    }
    break;
  case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    ESP_LOGI("gap_event", "adv stop");
    break;
  case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    ESP_LOGI("gap_event", "update connection, status %d, conn_int = %d", param->update_conn_params.status, param->update_conn_params.conn_int);
  default:
    break;
  }
}
 
static void service_a_event_handler (esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){
  switch (event)
  {
  case ESP_GATTS_REG_EVT: {    
    service_tab[SERVICE_A].service_id.is_primary = true;
    service_tab[SERVICE_A].service_id.id.inst_id = 0x00;
    service_tab[SERVICE_A].service_id.id.uuid.len = ESP_UUID_LEN_16;
    service_tab[SERVICE_A].service_id.id.uuid.uuid.uuid16 = SERVICE_A_UUID;
 
    esp_ble_gap_set_device_name(DEVICE_NAME);
 
    esp_ble_gap_config_adv_data(&adv_data);
 
    adv_config_done |= adv_config_flag;
    esp_ble_gap_config_adv_data(&scan_rsp_data);
    adv_config_done |= scan_rsp_config_flag;
 
    esp_ble_gatts_create_service(gatts_if, &service_tab[SERVICE_A].service_id, GATTS_NUM_HANDLE_A);
    }
    break;
 
  case ESP_GATTS_READ_EVT:{
    ESP_LOGI("service a", "Read request");
    esp_gatt_rsp_t response;
    memset(&response, 0, sizeof(esp_gatt_rsp_t));
    response.attr_value.handle = param->read.handle;
    response.attr_value.len = 4; 
    response.attr_value.value[0] = 0xAA;
    response.attr_value.value[1] = 0x00;
    response.attr_value.value[2] = 0x01;
    response.attr_value.value[3] = 0x02; 
    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &response);
  }
  break;
 
  case ESP_GATTS_WRITE_EVT:
    if (param->write.handle == service_tab[SERVICE_A].char_handle) {
    
    } else if (param->write.handle == service_tab[SERVICE_A].led_char_handle) {
        bool new_state = (param->write.value[0] != 0);
        xQueueSend(led_queue, &new_state, portMAX_DELAY);
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
    }
    break;


 
  case ESP_GATTS_CREATE_EVT:
    ESP_LOGI("service a", "create event");
    service_tab[SERVICE_A].service_handle = param->create.service_handle;
    service_tab[SERVICE_A].service_id.id.inst_id = 0x00; // Assuming instance ID is 0

    // Start the service
    esp_ble_gatts_start_service(service_tab[SERVICE_A].service_handle);

    // Add Potentiometer Characteristic
    esp_bt_uuid_t potentiometer_char_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = SERVICE_A_CHAR_1_UUID}
    };
    esp_gatt_perm_t pot_perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
    esp_gatt_char_prop_t pot_prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
    esp_ble_gatts_add_char(service_tab[SERVICE_A].service_handle, &potentiometer_char_uuid, pot_perm, pot_prop, &service_a_char_1_val, NULL);

    // Add LED Characteristic
    esp_bt_uuid_t led_char_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = LED_CHAR_UUID}
    };
    esp_gatt_perm_t led_perm = ESP_GATT_PERM_WRITE;
    esp_gatt_char_prop_t led_prop = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
    esp_ble_gatts_add_char(service_tab[SERVICE_A].service_handle, &led_char_uuid, led_perm, led_prop, NULL, NULL);

    break;

 
    esp_ble_gatts_add_char(service_tab[SERVICE_A].service_handle, &service_tab[SERVICE_A].char_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE, &service_a_char_1_val,NULL);
    break;
 
  case ESP_GATTS_ADD_CHAR_EVT:{
    ESP_LOGI("service a", "add char event");
    uint16_t length = 0;
    const uint8_t *prf_char;
 
    service_tab[SERVICE_A].char_handle = param->add_char.attr_handle;
    service_tab[SERVICE_A].descr_uuid.len = ESP_UUID_LEN_16;
    service_tab[SERVICE_A].descr_uuid.uuid.uuid16 =  ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
    esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &prf_char);
 
    esp_ble_gatts_add_char_descr(service_tab[SERVICE_A].service_handle, &service_tab[SERVICE_A].descr_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
  }
  break;
 
  case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    ESP_LOGI("service a", "add char descr");
    service_tab[SERVICE_A].descr_handle = param->add_char_descr.attr_handle;
    break;
 
  case ESP_GATTS_CONNECT_EVT:{
    ESP_LOGI("service a", "connection event");
    esp_ble_conn_update_params_t conn_params = {0};
    memcpy (conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
    conn_params.latency = 0;
    conn_params.max_int = 0x20;
    conn_params.min_int = 0x10;
    conn_params.timeout = 400;
 
    service_tab[SERVICE_A].conn_id = param->connect.conn_id;
 
    esp_ble_gap_update_conn_params(&conn_params);
  }
  break;
 
  case ESP_GATTS_DISCONNECT_EVT:
    esp_ble_gap_start_advertising(&adv_param);
    break;
 
  case ESP_GATTS_CONF_EVT:
    if(param->conf.status != ESP_GATT_OK){
      ESP_LOGI("service a", "Conf not ok");
    }
    break;
  default:
    break;
  }
}
 
static void service_b_event_handler (esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){
 
}

