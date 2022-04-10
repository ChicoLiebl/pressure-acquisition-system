/**
 * @file wifi.c
 * 
 * @brief Implementation of WIFI connection
 *
 * @author Francisco Eduardo Liebl
 * @contact: chicolliebl@gmail.com
 */
#include <string.h>
#include "esp_netif.h"
#include "network_wifi.h"
#include "esp_log.h"

#define DEBUG_LOG 1

#if DEBUG_LOG
#define WIFI_LOG(format, ... ) ESP_LOGW("WIFI", format, ##__VA_ARGS__)
#else
#define WIFI_LOG(format, ... )
#endif

static wifi_callbacks_t *global_wifi_callbacks = NULL;
static esp_netif_t *global_wifi_netif = NULL;
static bool wifi_running = false;

static void scan_done_handler() {
  uint16_t len = 0;  
  esp_wifi_scan_get_ap_num(&len);  
  if (len == 0 && global_wifi_callbacks->on_scan_result != NULL) 
    return global_wifi_callbacks->on_scan_result(0, NULL);
  wifi_ap_record_t scan_result[len];

  esp_err_t ret = esp_wifi_scan_get_ap_records(&len, scan_result);  
  for (int i = 0; i < len; i++) {
    printf("SSID:%s\n", scan_result[i].ssid);
  }
  if (ret == ESP_OK && global_wifi_callbacks->on_scan_result != NULL) {            
    global_wifi_callbacks->on_scan_result(len, scan_result);    
  }
}

static IRAM_ATTR void event_handler (
  void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data
) {
  if (event_base == WIFI_EVENT) {    
    switch (event_id) {
      // wifi ready
      case WIFI_EVENT_STA_START: {        
        ESP_ERROR_CHECK(wifi_scan());        
        WIFI_LOG("Start");
      } break;
      // disconnection
      case WIFI_EVENT_STA_STOP:
      case WIFI_EVENT_STA_DISCONNECTED:
        if (global_wifi_callbacks->on_disconnection != NULL)
          global_wifi_callbacks->on_disconnection();
        break;
      // scan finished
      case WIFI_EVENT_SCAN_DONE:
        WIFI_LOG("Scan done");
        scan_done_handler();        
        break;
      default: break;
    }
  } else if (event_base == IP_EVENT) {    
    switch (event_id) {
      case IP_EVENT_GOT_IP6: {        
        ip_event_got_ip6_t* event = (ip_event_got_ip6_t*) event_data;
        global_wifi_callbacks->on_connection(NULL, &event->ip6_info);
        break;
      }
      case IP_EVENT_STA_GOT_IP: {        
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        global_wifi_callbacks->on_connection(&event->ip_info, NULL);
        break;
      }
      default: break;
    }
  }  
  return;
}

void wifi_init (wifi_callbacks_t *config) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  global_wifi_callbacks = config;  

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

esp_err_t wifi_start_and_scan () { 
  if (!global_wifi_netif) global_wifi_netif = esp_netif_create_default_wifi_sta();
  ESP_LOGI("NETWORK", "Wifi start");
  // start wifi, scan is done in scan_done_handler()
  esp_err_t err = esp_wifi_start();
  if (err == ESP_OK) wifi_running = true;
  else esp_wifi_stop();
  return err;
}

esp_err_t wifi_scan () {  
  wifi_scan_config_t scan_config = { 0 };
  return esp_wifi_scan_start(&scan_config, false);
}

void wifi_connect (const char *ssid, const char *password) {
  wifi_config_t wifi_config = { 0 };
  strcpy((char*) wifi_config.sta.ssid, ssid);
  strcpy((char*) wifi_config.sta.password, password);
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_connect());
}

void wifi_stop () {
  if (!wifi_running) return;
  wifi_running = false;
  esp_err_t err = esp_wifi_stop();
  switch (err) {
    case ESP_ERR_WIFI_STOP_STATE: break; // ignore
    default: ESP_ERROR_CHECK(err);
  }
}

wifi_net_t* wifi_search_ssid (uint16_t scan_len, wifi_scan_item *scan_result, uint16_t config_len, wifi_net_t *config_list) {
  if (scan_len == 0) return NULL;

  // scan results
  for (uint16_t s = 0; s < scan_len; s++) {
    // configurations
    for (size_t c = 0; c < config_len; c++) {
      if (strcmp((char*) scan_result[s].ssid, config_list[c].ssid) == 0) {
        return &config_list[c];
      }
    }
  }

  return NULL;
}
