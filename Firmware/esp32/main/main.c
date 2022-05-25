#include <stdio.h>
#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "network_wifi.h"

#include "tcp_server.h"
#include "ble_conn/ble_server.h"
#include "configuration.h"

#include "ads8689.h"

static bool wifi_connected = false;

static void on_wifi_connection (esp_netif_ip_info_t *ipv4, esp_netif_ip6_info_t *ipv6) {
  const char ip[128];
  if (ipv4) sprintf(ip, IPSTR, IP2STR(&ipv4->ip));
  else sprintf(ip, IPV6STR, IPV62STR(ipv6->ip));
  
  ESP_LOGI("WIFI", "connected, IP: %s", ip);
  
  wifi_connected = true;
  ble_server_notify_ip(ip, strlen(ip));
  ble_server_notify_net_status(true);
  return;
}

static void on_wifi_scan_result (uint16_t len, wifi_scan_item *scan_result) {
  Configuration *curr_conf = configuration_get_current();
  wifi_net_t *configured_nets = malloc(sizeof(wifi_net_t) * curr_conf->n_networks);
  for (int i = 0; i < curr_conf->n_networks; i++) {
    configured_nets[i].ssid = curr_conf->networks[i]->ssid;
    configured_nets[i].password = curr_conf->networks[i]->password;
    printf("Saved net: %s\n", configured_nets[i].ssid);
  }

  wifi_net_t *net = wifi_search_ssid(len, scan_result, curr_conf->n_networks, configured_nets);
  if (net == NULL) {
    ESP_LOGW("NETWORK", "no configured wifi found");
    free(configured_nets);
    return;
  }

  wifi_connect(net->ssid, net->password);
  free(configured_nets);
  return;
}

void on_wifi_disconnet () {
  wifi_connected = false;
  ble_server_notify_net_status(false);
}

wifi_callbacks_t wifi_callbacks = {
  .on_scan_result = on_wifi_scan_result,
  .on_connection = on_wifi_connection,
  .on_disconnection = on_wifi_disconnet
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

void test_tcp_task () {
  #define TEST_LEN (1024)
  int16_t test_data[TEST_LEN];

  float fs = 100e3;
  float f = 16;

  float fbin = f / fs;

  float ph = 0;
  float fr  = 2 * M_PI * fbin;

  while (1) {
    for (int i = 0; i < TEST_LEN; i++) {
      test_data[i] = (int16_t) (INT16_MAX * sin(ph));
      ph += fr;
    }
    tcp_server_send_sync((uint8_t*) test_data, TEST_LEN * sizeof(int16_t));
    vTaskDelay(1);
  }
}

#define SEND_BUFFER_LEN (512)
#define CIRCULAR_BUFFER_LEN (SEND_BUFFER_LEN * 16)

static bool setup_done = false;

void adc_setup_task () {

  spi_bus_config_t spi_bus_cfg = {
    .mosi_io_num = GPIO_NUM_23,
    .miso_io_num = GPIO_NUM_19,
    .sclk_io_num = GPIO_NUM_18,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
  };

  ads8689_init(spi_bus_cfg, GPIO_NUM_5, SPI2_HOST);

  /* Set input range to 1.25 * Vref */
  ads8689_transmit(ADS8689_WRITE_LS, ADS8689_RANGE_SEL_REG, 0x0003, NULL, 0);
  
  ads8689_transmit(ADS8689_WRITE_LS, ADS8689_SDO_CTL_REG, 0x3 << 8, NULL, 0);

  ads8689_start_stream(CIRCULAR_BUFFER_LEN, 100000);
  setup_done = true;
  vTaskDelete(NULL);
}

void adc_read_task () {
  int16_t buffer[SEND_BUFFER_LEN];
  float fs;
  int64_t counter = 0, countFail = 0;
  while (1) {
    size_t read_len = ads8689_read_buffer(buffer, SEND_BUFFER_LEN, &fs);
    // if (read_len == 0) {
    //   // vTaskDelay(1);
    //   continue;
    // }
    if (!tcp_server_send_sync((uint8_t*) buffer, read_len * sizeof(uint16_t))) {
      vTaskDelay(10);
      // vTaskDelay(pdMS_TO_TICKS(10));
    } else {
      // vTaskDelay(1);
      if (read_len == SEND_BUFFER_LEN) {
        countFail++;
      }
      counter++;
    }
    if (counter == 1000) {
      printf("[%f]\tread len: %d\n", 100.f * (float) countFail / (float) counter , read_len);
      counter = 0;
      countFail = 0;
    }
  }
}

void on_tcp_connection () {
  ble_server_stop();
}

void app_main (void) {
  nvs_init();

  configuration_init();

  /* Setup reset GPIO */
  gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_26, 1);

  ble_server_start();

  wifi_init(&wifi_callbacks);
  wifi_start_and_scan();

  while (wifi_connected == false) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI("WIFI", "Waiting connection");
  }
  tcp_server_init(on_tcp_connection);
  
  // xTaskCreatePinnedToCore(test_tcp_task, "Test Task", 8192, NULL, 10, NULL, 1);

  xTaskCreatePinnedToCore(adc_setup_task, "ADC setup", 32 * 1024, NULL, 10, NULL, 1);
  while(!setup_done);
  xTaskCreatePinnedToCore(adc_read_task, "ADC read", 16 * 1024, NULL, 10, NULL, 0);
  printf("Main done!\n");
}
