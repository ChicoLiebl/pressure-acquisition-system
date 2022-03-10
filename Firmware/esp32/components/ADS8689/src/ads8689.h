#ifndef ADS8689_H
#define ADS8689_H

#include "driver/gpio.h"
#include "driver/spi_common.h"

/* Register mapping */
typedef enum ads8689_reg_t {
  ADS8689_DEVICE_ID_REG    = 0x00,
  ADS8689_RST_PWRCTL_REG   = 0x04,
  ADS8689_SDI_CTL_REG      = 0x08,
  ADS8689_SDO_CTL_REG      = 0x0C,
  ADS8689_DATAOUT_CTL_REG  = 0x10,
  ADS8689_RANGE_SEL_REG    = 0x14,
  ADS8689_ALARM_REG        = 0x20,
  ADS8689_ALARM_H_TH_REG   = 0x24,
  ADS8689_ALARM_L_TH_REG   = 0x28
} ads8689_reg_t;

/* Commands */
typedef enum ads8689_commands_t {
  ADS8689_NOP          = 0b0000000,
  ADS8689_CLEAR_HWORD  = 0b1100000,
  ADS8689_READ_HWORD   = 0b1100100,
  ADS8689_READ         = 0b0100100,
  ADS8689_WRITE_FULL   = 0b1101000,
  ADS8689_WRITE_MS     = 0b1101001,
  ADS8689_WRITE_LS     = 0b1101010,
  ADS8689_SET_HWORD    = 0b1101100
} ads8689_commands_t;

#ifdef __cplusplus
extern "C"
{
#endif

/* Init SPI driver for configuration */
esp_err_t ads8689_init (spi_bus_config_t spi_config, gpio_num_t cs_gpio, spi_host_device_t spi_host_id);

/* Transmit commands to the device */
esp_err_t ads8689_transmit (
  ads8689_commands_t command, ads8689_reg_t address, 
  uint16_t data_write, uint8_t *data_read, size_t read_len
);

void ads8689_start_stream (size_t buffer_len);

size_t ads8689_read_buffer (uint16_t *dest, size_t max_len);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif