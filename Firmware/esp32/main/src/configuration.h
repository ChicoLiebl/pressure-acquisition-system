#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "esp_err.h"
#include "configuration.pb-c.h"

/**
 * @brief Mounts configuration flash partition and search for saved configuration.
 * If saved configuration is not found, default configuration is saved to flash.
 */
void configuration_init ();

/** 
 * @brief Parse and save configuration from protobuf
 */
void configuration_parse_protobuf (uint8_t *payload, size_t len);

/** 
 * @brief save a Configuration message defined in protobuf into a flash partition 
 */
esp_err_t configuration_save_to_flash (Configuration *configuration);

/**
 * @brief load saved sensor configuration from flash
 * @returns NULL if file not found 
 */
Configuration* configuration_load_from_flash ();

/**
 * @brief get current configuration struct pointer
 * @returns pointer to current configuration struct 
 */
Configuration* configuration_get_current ();

/**
 * @brief add a wifi network to configuration
 */
void configuration_add_wifi_network (WifiNetwork *net);

/**
 * @brief add a wifi network to configuration
 */
void configuration_remove_wifi_network (WifiNetwork *net);

/**
 * @brief set sensor nickname in configuration
 */
void configuration_set_nickname (char *nick);

#endif