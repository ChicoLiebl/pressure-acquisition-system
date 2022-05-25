#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BLE_GATT_READ_PERM BLE_GATT_CHR_F_READ
#define BLE_GATT_WRITE_PERM BLE_GATT_CHR_F_WRITE

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

#endif // BLE_SERVER_H