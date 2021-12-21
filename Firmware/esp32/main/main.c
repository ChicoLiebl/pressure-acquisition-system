#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "network_wifi.h"

// #include "broadcast.h"
#include "tcp_server.h"

// const char ssid[] = "CLARO_2GDFA429";
// const char passwd[] = "B8DFA429";

const char ssid[] = "tracktum-setup";
const char passwd[] = "tracktum-1234";

static bool wifi_connected = false;

static void on_wifi_connection (esp_netif_ip_info_t *ipv4, esp_netif_ip6_info_t *ipv6) {
  if (ipv4) ESP_LOGI("WIFI", "connected, IPV4:" IPSTR, IP2STR(&ipv4->ip));
  else ESP_LOGI("WIFI", "connected, IPV6:" IPV6STR, IPV62STR(ipv6->ip));
  
  wifi_connected = true;

  return;
}

wifi_callbacks_t wifi_callbacks = {
  .on_scan_result = NULL,
  .on_connection = on_wifi_connection,
  .on_disconnection = NULL
};

static void nvs_init() {
  /* Initialize NVS */
  esp_err_t ret;
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void app_main(void) {
  nvs_init();

  wifi_init(&wifi_callbacks);
  wifi_start_and_scan();
  wifi_connect(ssid, passwd);

  while (wifi_connected == false) vTaskDelay(pdMS_TO_TICKS(100));
  tcp_server_init();

  char test_data[1024];

  for (int i = 0; i < 1024; i++) {
    test_data[i] = i % 255;
  }

  while (1) {
    send_tcp_packet((uint8_t*) test_data, 1024);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  
}
