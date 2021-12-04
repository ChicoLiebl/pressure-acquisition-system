/**
 * @file wifi.h
 * 
 * @brief Implementation of WIFI connection
 *
 * @author Francisco Eduardo Liebl
 * @contact: chicolliebl@gmail.com
 */
#ifndef NETWORK_WIFI_H
#define NETWORK_WIFI_H

#include <stdint.h>
#include "esp_wifi.h"

typedef wifi_ap_record_t wifi_scan_item;

typedef void (*void_callback) ();
typedef void (*wifi_connection_callback) (esp_netif_ip_info_t *ipv4, esp_netif_ip6_info_t *ipv6);
typedef void (*scan_result_callback) (uint16_t len, wifi_scan_item *scan_result);

typedef struct {
  char *ssid;
  char *password;
} wifi_net_t;

typedef struct wifi_callbacks_t {
  scan_result_callback on_scan_result;
  wifi_connection_callback on_connection;
  /** Disconnection, wrong ssid, invalid password */
  void_callback on_disconnection;
} wifi_callbacks_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize wifi callbacks struct 
 */
void wifi_init (wifi_callbacks_t *config);

/**
 * @brief Starts wifi modem, which automaticaly scans when stared 
 */
esp_err_t wifi_start_and_scan ();

/** 
 * @brief Retry scan. Should be used only wifi was previously started 
 */
esp_err_t wifi_scan ();


void wifi_connect (const char *ssid, const char *password);
void wifi_stop ();

/**
 * @brief Uses the wifi list from configuration to search the scan result
 * for a ssid that is configured
 * @returns found item pointer or NULL
 */
wifi_net_t* wifi_search_ssid (uint16_t scan_len, wifi_scan_item *scan_result, uint16_t config_len, wifi_net_t *config_list);

#ifdef __cplusplus
}
#endif

#endif