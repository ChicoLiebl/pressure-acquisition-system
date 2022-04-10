#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_system.h"

#include "configuration.h"

/* Default net for emergency cases */

// static char def_ssid[] = "sensor_setup";
// static char def_pwd[] = "pascal";

const char def_ssid[] = "CLARO_2GDFA429";
const char def_pwd[] = "B8DFA429";

// const char def_ssid[] = "tracktum-setup";
// const char def_pwd[] = "tracktum-1234";

// const char def_ssid[] = "tracktum";
// const char def_pwd[] = "dispositivosseguros";

// const char def_ssid[] = "SchoeffelLiebl";
// const char def_pwd[] = "30041988";

static WifiNetwork default_net = {
  .base = PROTOBUF_C_MESSAGE_INIT(&wifi_network__descriptor),
  .ssid = def_ssid,
  .password = def_pwd
};

static WifiNetwork* networks[10] = {};

static char default_nick[] = "PressureSensor";

static Configuration default_config = {
  .base = PROTOBUF_C_MESSAGE_INIT(&configuration__descriptor),
  .nickname = default_nick,
  .n_networks = 0,
  .networks = networks,
};

static Configuration global_config = {};

static const char *base_path = "/conf";

static const char *TAG = "CONFIGURATION";

void configuration_init () {
  ESP_LOGI(TAG, "Mounting SPIFFS filesystem");

  esp_vfs_spiffs_conf_t spiffs_conf = {
    .base_path = "/conf",
    .format_if_mount_failed = true,
    .max_files = 5,
    .partition_label = "config"
  };

  esp_err_t err = esp_vfs_spiffs_register(&spiffs_conf);

  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(err));
      return;
  }
  ESP_LOGI(TAG, "Reading configuration file");
  Configuration *read_conf = configuration_load_from_flash();
  if (read_conf == NULL) {
    ESP_LOGI(TAG, "No configuration file found");
    networks[0] = &default_net;
    global_config = default_config;
    global_config.n_networks = 1;
    ESP_LOGI(TAG, "Saving default configuration to flash");
    configuration_save_to_flash(&global_config);
  } else {
    global_config = *read_conf;
    free(read_conf);
    ESP_LOGI(TAG, "Using configuration from flash");
  }
}

/* Writes a protobuf message in a file with its name */
static esp_err_t write_proto_message_to_file (ProtobufCMessage *configuration) {
  /** Defines file name according to Protobuf definition */

  size_t file_name_len = strlen(configuration->descriptor->name) + sizeof(".conf") + strlen(base_path) + 1;
  char file_name[file_name_len];
  strcpy(file_name, base_path);
  strcat(file_name, "/");
  strcat(file_name, configuration->descriptor->name);
  strcat(file_name, ".conf");

  uint16_t payload_len = protobuf_c_message_get_packed_size(configuration);
  uint8_t *payload = (uint8_t*) malloc(payload_len);
  protobuf_c_message_pack(configuration, payload);

  FILE* f = fopen(file_name, "wb");
  if (f == NULL) {
    ESP_LOGE(TAG, "Couldn't open file for writing");
    return -1;
  }
  fwrite(&payload_len, 1, sizeof(uint16_t), f);
  fwrite(payload, payload_len, 1, f);

  fclose(f);
  free(payload);
  return ESP_OK;
}

/* Reads a protobuf message from a file with its name, if file exists */
static ProtobufCMessage* read_proto_message_from_file (const ProtobufCMessageDescriptor *descriptor) {
  ProtobufCMessage *configuration;
  /** Defines file name according to Protobuf definition */
  size_t file_name_len = strlen(descriptor->name) + sizeof(".conf") + strlen(base_path) + 1;
  char file_name[file_name_len];
  strcpy(file_name, base_path);
  strcat(file_name, "/");
  strcat(file_name, descriptor->name);
  strcat(file_name, ".conf");
  FILE* f = fopen(file_name, "rb");
  if (f == NULL) {
    ESP_LOGE(TAG, "File not found");
    return NULL;
  }
  uint16_t payload_len;
  fread(&payload_len, 1, sizeof(uint16_t), f);
  uint8_t *payload = (uint8_t*) malloc(payload_len);
  fread(payload, payload_len, 1, f);
  fclose(f);

  configuration = protobuf_c_message_unpack(descriptor, NULL, payload_len, payload);
  free(payload);
  return configuration;
}

void configuration_add_wifi_network (WifiNetwork *net) {
  int index = 0;
  bool same_net = false;
  /* Verify if network was already in configuration */
  for (int i = 0; i < global_config.n_networks; i++) {
    if (strcmp(net->ssid, global_config.networks[i]->ssid) == 0) {
      /* Found same network, update pasword */
      char *new_pswd = (char*) malloc(strlen(net->ssid)); 
      strcpy(new_pswd, net->password);
      free(global_config.networks[i]->ssid);
      global_config.networks[i]->ssid = new_pswd;
      same_net = true;
      break;
    }
  }
  if (!same_net) {
    if (global_config.n_networks == 10) {
      free(global_config.networks[9]->ssid);
      free(global_config.networks[9]->password);
      free(global_config.networks[9]);
      memmove(global_config.networks + 2, global_config.networks + 1, sizeof(WifiNetwork*) * 8);
      index = 1;
    } else {
      index = global_config.n_networks;
      printf("%d\n", index);
      global_config.n_networks++;
    }
    char *ssid = (char*) malloc(strlen(net->ssid) + 1); 
    char *password = (char*) malloc(strlen(net->password) + 1);

    strcpy(ssid, net->ssid);
    strcpy(password, net->password);
    
    WifiNetwork *new_net = (WifiNetwork*) malloc(sizeof(WifiNetwork));
    wifi_network__init(new_net);
    new_net->ssid = ssid;
    new_net->password = password;
    global_config.networks[index] = new_net;
  }

  configuration_save_to_flash(&global_config);
}

void configuration_remove_wifi_network (WifiNetwork *net) {
  int index = 0;
  bool same_net = false;
  /* Search for network */
  for (int i = 0; i < global_config.n_networks; i++) {
    if (strcmp(net->ssid, global_config.networks[i]->ssid) == 0) {
      /* Free net */
      free(global_config.networks[9]->ssid);
      free(global_config.networks[9]->password);
      free(global_config.networks[9]);
      if (i != (global_config.n_networks - 1)){
        memmove(global_config.networks + i, global_config.networks + (i + 1), sizeof(WifiNetwork*) * (global_config.n_networks - (i + 1)));
      }
      global_config.n_networks--;
      break;
    }
  }
  configuration_save_to_flash(&global_config);
}

void configuration_set_nickname (char *nick) {
  char *new_nick = (char*) malloc(strlen(nick));
  strcpy(new_nick, nick);
  if (global_config.nickname != NULL) free(global_config.nickname);
  global_config.nickname = new_nick;
  configuration_save_to_flash(&global_config);
}

void configuration_parse_protobuf (uint8_t *payload, size_t len) {
  Configuration *received_conf = configuration__unpack(NULL, len, payload);
  if (received_conf == NULL) {
    ESP_LOGE("CONFIGURATION", "Failed to parse config");
    return;
  }
  configuration_save_to_flash(received_conf);
  ESP_LOGI("CONFIGURATION", "Updated, restarting system");
  esp_restart();
}

esp_err_t configuration_save_to_flash (Configuration *configuration) {
  return write_proto_message_to_file((ProtobufCMessage*) configuration);
}

Configuration* configuration_load_from_flash () {
  return (Configuration*) read_proto_message_from_file(&configuration__descriptor);
}

Configuration* configuration_get_current () {
  return &global_config;
}
