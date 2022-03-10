#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "network_wifi.h"

// #include "broadcast.h"
#include "tcp_server.h"

#include "ads8689.h"

const char ssid[] = "CLARO_2GDFA429";
const char passwd[] = "B8DFA429";

// const char ssid[] = "tracktum-setup";
// const char passwd[] = "tracktum-1234";

// const char ssid[] = "SchoeffelLiebl";
// const char passwd[] = "30041988";

static bool wifi_connected = false;

static void on_wifi_connection (esp_netif_ip_info_t *ipv4, esp_netif_ip6_info_t *ipv6) {
  if (ipv4) ESP_LOGI("WIFI", "connected, IPV4:" IPSTR, IP2STR(&ipv4->ip));
  else ESP_LOGI("WIFI", "connected, IPV6:" IPV6STR, IPV62STR(ipv6->ip));
  
  wifi_connected = true;

  return;
}

const char lorem_ipsum[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. \
  Sed metus sem, tincidunt eu rhoncus vel, vulputate finibus nulla. Suspendisse eleifend\
  purus et ipsum auctor, et congue massa semper. Ut dictum vitae turpis sit amet sodales. \
  Duis non lacus vel orci malesuada interdum. Praesent non tortor non metus efficitur finibus \
  faucibus in nunc. Donec placerat elit nec sapien elementum posuere. Cras gravida congue elit \
  nec ultricies. Nullam ipsum enim, consectetur non dictum sed, aliquam vitae nisl. \
  In efficitur justo at tortor aliquet interdum non a arcu. Phasellus aliquet sem vel \
  pharetra tempus.";

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

void throuput_test_task () {
  #define TEST_LEN (512)

  int16_t test_data[TEST_LEN];

  for (int i = 0; i < TEST_LEN; i++) {
    test_data[i] = i % INT16_MAX;
  }
  test_data[0] = 0;
  while (1) {
    test_data[0]++;
    send_tcp_packet((uint8_t*) test_data, TEST_LEN * sizeof(int16_t));
    // send_tcp_packet(lorem_ipsum, sizeof(lorem_ipsum));
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void integrit_test_task () {
  #define TEST_LEN (512)

  int16_t test_data[TEST_LEN];

  int counter = 0;
  while (1) {
    for (int i = 0; i < TEST_LEN; i++) {
      test_data[i] = INT16_MAX * sin((float) counter/10000);
      counter++;
    }
    send_tcp_packet((uint8_t*) test_data, TEST_LEN * sizeof(int16_t));
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void adc_read_test_task () {

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

  ads8689_start_stream(4096);
  
  vTaskDelete(NULL);
}

void app_main(void) {
  nvs_init();

  // wifi_init(&wifi_callbacks);
  // wifi_start_and_scan();
  // wifi_connect(ssid, passwd);

  // while (wifi_connected == false) vTaskDelay(pdMS_TO_TICKS(100));
  // tcp_server_init();

  // xTaskCreatePinnedToCore(integrit_test_task, "Integrit Test", 8192, NULL, 10, NULL, 1);
  // xTaskCreatePinnedToCore(throuput_test_task, "Throughput Test", 8192, NULL, 10, NULL, 1);

  xTaskCreatePinnedToCore(adc_read_test_task, "ADC Test", 8192, NULL, 10, NULL, 1);
}
