#ifndef TRACKTUM_BLE_SERVER_H
#define TRACKTUM_BLE_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BLE_SERVER_ENC 0

#if BLE_SERVER_ENC
	#define BLE_GATT_READ_PERM BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC
	#define BLE_GATT_WRITE_PERM BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC
#else
	#define BLE_GATT_READ_PERM BLE_GATT_CHR_F_READ
	#define BLE_GATT_WRITE_PERM BLE_GATT_CHR_F_WRITE
#endif

#ifdef __cplusplus
extern "C" {
#endif

void ble_server_start ();
void ble_server_stop ();
void ble_server_notify_ip (const char *ip_addr, const size_t len);
void ble_server_notify_net_status (bool value);

#ifdef __cplusplus
}
#endif

#endif // TRACKTUM_BLE_SERVER_H