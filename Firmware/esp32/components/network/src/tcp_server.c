#include <lwip/netdb.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "esp_log.h"

#include "tcp_server.h"

#define TAG "TCP SERVER"

#define PORT 3333

static TaskHandle_t tcp_server_task_handle;
static QueueHandle_t tcp_server_queue;

static bool client_connected = false;
static struct sockaddr_in6 client_addr;  // Large enough for both IPv4 or IPv6

static int listen_socket;

static void get_ip_string (struct sockaddr_in6 *addr, char *addr_string) {
  // Get the sender's ip address as string
  if (addr->sin6_family == PF_INET) {
    inet_ntoa_r(((struct sockaddr_in *)addr)->sin_addr.s_addr, addr_string, 128 - 1);
  } else if (addr->sin6_family == PF_INET6) {
    inet6_ntoa_r(addr->sin6_addr, addr_string, 128 - 1);
  }
}

static bool accept_connection (const int socket) {
  size_t len;
  char rx_buffer[128];

  len = recv(socket, rx_buffer, sizeof(rx_buffer) - 1, 0);
  if (len < 0) {
    ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
    return false;
  } else if (len == 0) {
    ESP_LOGW(TAG, "Connection closed");
    return false;
  } else {
    printf("%s\n", rx_buffer);
    return (strcmp("connection_request", rx_buffer) == 0)? true : false;
  }
}

static void clean_up (int socket) {
  close(socket);
}

static void tcp_server_task () {
  char rx_buffer[128];
  char addr_str[128];
  int addr_family = AF_INET;
  int ip_protocol = 0;
  struct sockaddr_in6 dest_addr;
  static struct sockaddr_in6 recv_addr;

  struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
  dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr_ip4->sin_family = AF_INET;
  dest_addr_ip4->sin_port = htons(PORT);
  ip_protocol = IPPROTO_IP;

  listen_socket = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (listen_socket < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }
  ESP_LOGI(TAG, "Socket created");

  int err = bind(listen_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
    ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
  }
  ESP_LOGI(TAG, "Socket bound, port %d", PORT);

  /* Socket listen loop */
  while (1) {
    err = listen(listen_socket, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        clean_up(listen_socket);
        break;
    }
    ESP_LOGI(TAG, "Listen started");

    struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
    uint addr_len = sizeof(source_addr);
    int sock = accept(listen_socket, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        // break;
    }

    get_ip_string(&recv_addr, addr_str);

    if (!accept_connection(sock)) continue;
    
    ESP_LOGI(TAG, "Broadcast started");
    client_connected = true;

    /* Send loop */
    broadcast_message_t msg;
    while (1) {
      if (xQueueReceive(tcp_server_queue, &msg, (TickType_t) 10)) {
        int written = send(sock, msg.data, msg.len, 0);
        free(msg.data);
        if (written < 0) {
          ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
          break;
        }
      }
    }

    client_connected = false;
  }
  close(listen_socket);
  vTaskDelete(NULL);
}

void tcp_server_init () {
  tcp_server_queue = xQueueCreate(10, sizeof(broadcast_message_t));
  xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL, 0);
}

void send_tcp_packet (uint8_t *data, size_t len) {
  if (!client_connected) return;
  broadcast_message_t msg;
  /* Deep copy the message */
  msg.data = (uint8_t*) malloc(len);
  msg.len = len;
  memcpy(data, msg.data, len);

  xQueueSend(tcp_server_queue, &msg, (TickType_t) 100);
}