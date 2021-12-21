idf_component_register(
  SRCS "src/network_wifi.c" "src/tcp_server.c"
  INCLUDE_DIRS "src/"
  REQUIRES 
    nvs_flash
)
