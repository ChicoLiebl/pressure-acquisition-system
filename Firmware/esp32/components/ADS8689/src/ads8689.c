#include <string.h>

#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ads8689.h"

#define LOG_TAG "ADS8689"

static uint32_t *receive_buffer = NULL;

static spi_device_handle_t spi_handle = NULL;

#define SPI_MOSI_BUF_SIZE 128
#define SPI_MISO_BUF_SIZE 128

/* Mosi and miso buffers */
static uint8_t *mosi_buffer = NULL;
static uint8_t *miso_buffer = NULL;

static SemaphoreHandle_t spi_mutex;

esp_err_t ads8689_init (uint8_t buffer_size, spi_bus_config_t spi_config, gpio_num_t cs_gpio, spi_host_device_t spi_host_id) {
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_MASTER_FREQ_40M,   // clock speed
    .mode = SPI_TRANS_MODE_DIO,              // SPI mode 0
    .spics_io_num = cs_gpio,                 // CS GPIO
    .queue_size = 1,
    .flags = 0,                              // no flags set
    .command_bits = 0,                       // no command bits used
    .address_bits = 0,                       // register address is first byte in MOSI
    .dummy_bits = 0                          // no dummy bits used
  };
  
  esp_err_t ret = spi_bus_initialize(spi_host_id, &spi_config, 1);

  ret |= spi_bus_add_device(spi_host_id, &dev_cfg, &spi_handle);

  ret |= gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);
  ret |= gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT_OD);
  ret |= gpio_set_level(GPIO_NUM_15, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  ret |= gpio_set_level(GPIO_NUM_15, 1);
  

  if (ret != ESP_OK) return ret;

  /* Alloc buffers */
  receive_buffer = (uint32_t*) malloc(buffer_size * sizeof(uint32_t));

  mosi_buffer = (uint8_t*) heap_caps_malloc(SPI_MOSI_BUF_SIZE, MALLOC_CAP_DMA);
  miso_buffer = (uint8_t*) heap_caps_malloc(SPI_MISO_BUF_SIZE, MALLOC_CAP_DMA);

  spi_mutex = xSemaphoreCreateMutex();

  return ret;
}

esp_err_t ads8689_transmit (
  ads8689_commands_t command, ads8689_reg_t address, 
  uint16_t data_write, uint8_t *data_read, size_t read_len
) {
  memset(mosi_buffer, 0xff, SPI_MOSI_BUF_SIZE);

  mosi_buffer[0] = (command<<1)|((address>>8)&1);
  mosi_buffer[1] = (address&0xFF);
  mosi_buffer[2] = ((data_write>>8)&0xFF);
  mosi_buffer[3] = (data_write&0xFF);

  spi_transaction_t spi_trans = {
    .tx_buffer = mosi_buffer,
    .rx_buffer = miso_buffer,
    .length = (4 + read_len) * 8
  };

  xSemaphoreTake(spi_mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_handle, &spi_trans);
  xSemaphoreGive(spi_mutex);
  
  if (ret != ESP_OK) ESP_LOGE(LOG_TAG, "Failed to transmit command %#x, address %#x", command, address);
  memcpy(data_read, miso_buffer, read_len);

  return ret;
}

void read_device_id () {
  uint16_t id;
  ads8689_transmit(ADS8689_READ_HWORD, ADS8689_RST_PWRCTL_REG, 0, (uint8_t*) &id, sizeof(uint16_t));
  printf("Read device ID %#x\n", id);
}

uint16_t IRAM_ATTR read_conversion_data () {
  spi_transaction_t spi_trans = {
    .tx_buffer = NULL,
    .rx_buffer = miso_buffer,
    .length = 2 * 8
  };

  xSemaphoreTake(spi_mutex, portMAX_DELAY);
  // esp_err_t ret = spi_device_transmit(spi_handle, &spi_trans);
  esp_err_t ret = spi_device_polling_transmit(spi_handle, &spi_trans);
  xSemaphoreGive(spi_mutex);

  return (miso_buffer[0] << 8 | miso_buffer[1]) - INT16_MAX;
}

// uint32_t ads8689_read_buffer () {

// }

// uint8_t ads8689_input_available () {

// }

// void ads8689_clear_buffer () {

// }
