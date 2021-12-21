#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdint.h>
#include <stddef.h>

typedef struct broadcast_message_t {
  uint8_t *data;
  size_t len;
} broadcast_message_t;

void tcp_server_init ();

esp_err_t send_tcp_packet (uint8_t *data, size_t len);

#endif