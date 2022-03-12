#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "hal/spi_hal.h"
#include "soc/spi_periph.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "ads8689.h"

#define LOG_TAG "ADS8689"

#define SPI_MOSI_BUF_SIZE 128
#define SPI_MISO_BUF_SIZE 128

#define min(x,y) ( \
    { __auto_type __x = (x); __auto_type __y = (y); \
      __x < __y ? __x : __y; })

#define PRINT_BUFFER(format, buffer, len) \
  for (int _i = 0; _i < len; _i++) { \
    printf(format, (buffer)[_i]); \
    if ((_i + 1) % 16 == 0) printf("\n"); \
  } \
  printf("\n");

/* Mosi and miso buffers */
static uint8_t *mosi_buffer = NULL;
static uint8_t *miso_buffer = NULL;

typedef struct circular_buffer_t {
  uint16_t *data;
  size_t len;
  size_t front;
  size_t rear;
} circular_buffer_t;

static circular_buffer_t receive_buffer;

/* SPI hardware variables */
static spi_host_device_t spi_host;
static spi_device_handle_t spi_handle;
static spi_hal_context_t spi_bus_hal;

/* Test variables */
int64_t start_time;
int64_t *read_time_us = NULL;

static void IRAM_ATTR spi_handler(void *arg) {
  spi_dev_t *dev = spi_bus_hal.hw;
  circular_buffer_t *buffer = &receive_buffer;
  ets_printf("spi intr: %d\n", dev->slave.trans_done);


  dev->slave.trans_done = 0; // reset the register
  uint32_t data = SPI_SWAP_DATA_RX(*(dev->data_buf), 16);
  size_t front = buffer->front;

  buffer->data[buffer->front] = data;

  /* Increments circular buffer front */
  buffer->front++;
  if (buffer->front == buffer->len) buffer->front = 0;
  /* if front reaches rear dump data and move rear */
  if (buffer->front == buffer->rear) {
    // ets_printf("full\n");
    buffer->rear++;
    if (buffer->rear == buffer->len) buffer->rear = 0;
  }
  read_time_us[front] = esp_timer_get_time() - start_time;
  // ets_printf("front %d, rear %d\n", receive_buffer.front, receive_buffer.rear);
}

esp_err_t ads8689_init (spi_bus_config_t spi_config, gpio_num_t cs_gpio, spi_host_device_t spi_host_id) {
  spi_host = spi_host_id;

  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_MASTER_FREQ_20M,   // clock speed
    .mode = SPI_TRANS_MODE_DIO,              // SPI mode 0
    .spics_io_num = cs_gpio,                 // CS GPIO
    .queue_size = 5,
    .flags = 0,                              // no flags set
    .command_bits = 0,                       // no command bits used
    .address_bits = 0,                       // register address is first byte in MOSI
    .dummy_bits = 0                          // no dummy bits used
  };
  
  /* Set RTS and RVS */
  esp_err_t ret = gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);
  ret |= gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT_OD);
  ret |= gpio_set_level(GPIO_NUM_15, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  ret |= gpio_set_level(GPIO_NUM_15, 1);
  
  /* Alloc buffers */
  mosi_buffer = (uint8_t*) heap_caps_malloc(SPI_MOSI_BUF_SIZE, MALLOC_CAP_DMA);
  miso_buffer = (uint8_t*) heap_caps_malloc(SPI_MISO_BUF_SIZE, MALLOC_CAP_DMA);

  ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &spi_config, 1));
  ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &dev_cfg, &spi_handle));
  ESP_ERROR_CHECK(spi_device_acquire_bus(spi_handle, portMAX_DELAY));

  spi_bus_hal = spi_bus_get_hal(spi_host);

  if (ret != ESP_OK) return ret;

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

  // xSemaphoreTake(spi_mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_handle, &spi_trans);
  // xSemaphoreGive(spi_mutex);
  
  if (ret != ESP_OK) ESP_LOGE(LOG_TAG, "Failed to transmit command %#x, address %#x", command, address);
  memcpy(data_read, miso_buffer, read_len);

  return ret;
}

static void IRAM_ATTR read_timer_callback (void *arg) {
  // ets_printf("timer intr: %lld, %d\n", esp_timer_get_time() - start_time, spi_bus_hal.hw->cmd.usr);
  // printf("read val = %.4f\n", (float) (test_data - INT16_MAX) * 4096 / INT16_MAX * 1.25 / 1000);
  start_time = esp_timer_get_time();
  spi_bus_hal.hw->cmd.usr = 1;
  // esp_timer_isr_dispatch_need_yield();
}

void ads8689_start_stream (size_t buffer_len, int64_t sample_freq) {
  if (sample_freq > 100000) {
    ESP_LOGE(LOG_TAG, "Invalid sample frequency, stream not started");
    return;
  }

  receive_buffer.len = buffer_len;
  receive_buffer.data = (uint16_t*) malloc(buffer_len * sizeof(uint16_t));
  read_time_us = (int64_t*) malloc(buffer_len * sizeof(int64_t));


  /* Switch spi trans_done interrupt for one locally defined */
  // free spi intr
  intr_handle_t spi_int = spi_bus_get_intr(spi_host);
  ESP_ERROR_CHECK(esp_intr_disable(spi_int));
  ESP_ERROR_CHECK(esp_intr_free(spi_int));
  
  // select intr signal
  int spi_intr_source = spi_periph_signal[spi_host].irq;
  esp_intr_enable_source(spi_intr_source);
  
  // alloc a new lv3 intr
  intr_handle_t spi_intr_handle;
  ESP_ERROR_CHECK(esp_intr_alloc(spi_intr_source, ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_INTRDISABLED, spi_handler, NULL, &spi_intr_handle));
  ESP_ERROR_CHECK(esp_intr_enable(spi_intr_handle));

  /* Create the reading timer */
  esp_timer_handle_t read_timer_handle;
  esp_timer_create_args_t read_timer_args = {
    .callback = &read_timer_callback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "ADS8689 acquire timer"
  };

  esp_timer_create(&read_timer_args, &read_timer_handle);
  printf("sample time %lld\n", 1000000 / sample_freq);
  esp_timer_start_periodic(read_timer_handle, 1000000 / sample_freq);
}

static void log_avg_acquisition_time (int64_t *read_time, size_t len) {
  float avg_period = 0;
  for (int i = 0; i < len; i++) {
    avg_period += read_time[i];
  }
  ESP_LOGI(LOG_TAG, "Avg aquisition time: %f", avg_period / len);
}

size_t ads8689_read_buffer (uint16_t *dest, size_t max_len) {
  circular_buffer_t *buffer = &receive_buffer;
  // printf("last read time = %lld\n", read_time_us[buffer->front - 1]);

  if (buffer->front == buffer->rear) return 0;

  if (buffer->front > buffer->rear) {
    // printf("front > rear\n");
    size_t read_len = min(max_len, buffer->front - buffer->rear);
    memcpy(dest, buffer->data + buffer->rear, read_len);
    // log_avg_acquisition_time(read_time_us + buffer->rear, read_len);
    // PRINT_BUFFER("%lld ", (read_time_us + buffer->rear), read_len);
    buffer->rear += read_len;
    return read_len;
  }

  /* If read > front, buffer is divided */
  // printf("front < rear\n");
  size_t end_len = (buffer->len - buffer->rear);
  if (end_len >= max_len) {
    memcpy(dest, buffer->data + buffer->rear, max_len);
    buffer->rear += max_len;
    if (buffer->rear == buffer->len) buffer->rear = 0;
    return max_len;
  }

  memcpy(dest, buffer->data + buffer->rear, end_len);
  size_t start_len = min(buffer->front, max_len - end_len);
  memcpy(dest + end_len, buffer->data, start_len);

  buffer->rear = start_len;

  return end_len + start_len;
}

