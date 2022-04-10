
#include "ble_conn/ble_server.h"
#include "configuration.h"

#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BLE_SERVER_LOG(format, ...) ESP_LOGW("BLE SRV", format, ##__VA_ARGS__)


#define UUID128_INIT(uuid128...)    \
    {                                   \
        .u = (ble_uuid_t) { .type = BLE_UUID_TYPE_128 },    \
        .value = { uuid128 },           \
    }
#define UUID16_INIT(uuid16)         \
    {                                   \
        .u = (ble_uuid_t) { .type = BLE_UUID_TYPE_16 },    \
        .value = (uuid16),              \
    }

// f3640000-00b0-4240-ba50-05ca45bf8abc
static const ble_uuid128_t base_uuid = UUID128_INIT(0xBC, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, 0x40, 0x42, 0xB0, 0x00, 0x00, 0x00, 0x64, 0xF3); // base
// f3641001-00b0-4240-ba50-05ca45bf8abc
static const ble_uuid128_t ip_id = UUID128_INIT(0xBC, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, 0x40, 0x42, 0xB0, 0x00, 0x01, 0x10, 0x64, 0xF3); // 0x1001
// f3641002-00b0-4240-ba50-05ca45bf8abc
static const ble_uuid128_t net_status_id = UUID128_INIT(0xBC, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, 0x40, 0x42, 0xB0, 0x00, 0x02, 0x10, 0x64, 0xF3); // 0x1002
// f3641010-00b0-4240-ba50-05ca45bf8abc
static const ble_uuid128_t nick_id = UUID128_INIT(0xBC, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, 0x40, 0x42, 0xB0, 0x00, 0x10, 0x10, 0x64, 0xF3); // 0x1010
// f3641020-00b0-4240-ba50-05ca45bf8abc
static const ble_uuid128_t configuration_id = UUID128_INIT(0xBC, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, 0x40, 0x42, 0xB0, 0x00, 0x20, 0x10, 0x64, 0xF3); // 0x1020
// f3641021-00b0-4240-ba50-05ca45bf8abc
static const ble_uuid128_t command_id = UUID128_INIT(0xBC, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, 0x40, 0x42, 0xB0, 0x00, 0x21, 0x10, 0x64, 0xF3); // 0x1021

static uint16_t ip_handle = 0, net_status_handle = 0, conn_handle = 0, configuration_handle = 0, command_handle = 0;

static bool ip_notification = false, net_status_notification = false;

static char curr_ip[128] = "DISCONNECTED";
static bool net_status = false;

void ble_server_notify_ip (const char *ip_addr, const size_t len) {
  strcpy(curr_ip, ip_addr);
  if (ip_notification && ip_handle != 0) {
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&ip_addr, len);
    ble_gattc_notify_custom(conn_handle, ip_handle, om);
  }
}

void ble_server_notify_net_status (bool value) {
  if (net_status_notification && net_status_handle != 0) {
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&value, sizeof(uint16_t));
    ble_gattc_notify_custom(conn_handle, net_status_handle, om);
  }
}

static int do_nothing_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) { return 0; }

static int read_ip_cb (uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  // if (curr_ip == NULL) {
  //   char no_ip[] = "DISCONNECTED";
  //   return os_mbuf_append(ctxt->om, (uint8_t*) no_ip, strlen(no_ip));
  // }
  return os_mbuf_append(ctxt->om, (uint8_t*) curr_ip, strlen(curr_ip));
}

static int read_net_status_cb (uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  return os_mbuf_append(ctxt->om, (uint8_t*) &net_status, 1);
}

static int read_configuration_cb (uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  Configuration* curr_conf = configuration_get_current();
  size_t len = configuration__get_packed_size(curr_conf);
  uint8_t *payload = malloc(len);
  configuration__pack(curr_conf, payload);
  
  int ret = os_mbuf_append(ctxt->om, payload, len);
  
  free(payload);
  return ret;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &base_uuid.u,
    .characteristics = (struct ble_gatt_chr_def[]) {
      {
        .uuid = &ip_id.u,
        .access_cb = read_ip_cb,
        .flags = BLE_GATT_READ_PERM | BLE_GATT_CHR_F_INDICATE,
        .val_handle = &ip_handle,
      },
      {
        .uuid = &net_status_id.u,
        .access_cb = read_net_status_cb,
        .flags = BLE_GATT_READ_PERM | BLE_GATT_CHR_F_INDICATE,
        .val_handle = &net_status_handle,
      },
      {
        .uuid = &nick_id.u,
        .access_cb = do_nothing_gatt_cb,
        .flags = BLE_GATT_READ_PERM,
      },
      {
        .uuid = &configuration_id.u,
        .access_cb = read_configuration_cb,
        .flags = BLE_GATT_READ_PERM,
      },
      {
        .uuid = &command_id.u,
        .access_cb = do_nothing_gatt_cb,
        .flags = BLE_GATT_WRITE_PERM | BLE_GATT_CHR_F_INDICATE,
      },
      { 0 }
    },
  },
  { 0 }
};

static uint8_t own_addr_type;
static int bleprph_gap_event(struct ble_gap_event *event, void *arg);

static void bleprph_on_reset(int reason) {
  BLE_SERVER_LOG("Resetting state; reason=%d\n", reason);
}

void print_addr(const void *addr) {
  const uint8_t *u8p = (const uint8_t*) addr;
  BLE_SERVER_LOG("%02x:%02x:%02x:%02x:%02x:%02x", u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

void bleprph_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_uuid16_t uuids16[] = {
      UUID16_INIT(0x1811)
    };
    fields.uuids16 = uuids16;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 500;
    adv_params.itvl_max = 2000;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static void bleprph_print_conn_desc(struct ble_gap_conn_desc *desc) {
    BLE_SERVER_LOG("handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    BLE_SERVER_LOG(" our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    BLE_SERVER_LOG(" peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    BLE_SERVER_LOG(" peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    BLE_SERVER_LOG(" conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}


static int bleprph_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    int rc;

    BLE_SERVER_LOG("EVENT %d", event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        BLE_SERVER_LOG("connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            conn_handle = desc.conn_handle;
            // BLE_SERVER_LOG("init sec=%d", ble_gap_security_initiate(event->connect.conn_handle));
            // TODO: update connection parameters, @see esp_ble_gap_update_conn_params(&conn_params);
            ble_gattc_exchange_mtu(conn_handle, NULL, NULL);
            bleprph_print_conn_desc(&desc);
        }
        BLE_SERVER_LOG("\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        BLE_SERVER_LOG("disconnect; reason=%d ", event->disconnect.reason);
        bleprph_print_conn_desc(&event->disconnect.conn);
        // touch_notification = false;
        // light_notification = false;
        BLE_SERVER_LOG("\n");

        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        BLE_SERVER_LOG("connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        BLE_SERVER_LOG("\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        BLE_SERVER_LOG("advertise complete; reason=%d",
                    event->adv_complete.reason);
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        BLE_SERVER_LOG("encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        BLE_SERVER_LOG("\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        BLE_SERVER_LOG("subscribe event; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        if (event->subscribe.attr_handle == ip_handle) {
          ip_notification = event->subscribe.cur_indicate || event->subscribe.cur_notify;
        } else if (event->subscribe.attr_handle == net_status_handle) {
          net_status_notification = event->subscribe.cur_indicate || event->subscribe.cur_notify;
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        BLE_SERVER_LOG("mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        BLE_SERVER_LOG("PASSKEY_ACTION_EVENT started \n");
        return 0;
    }

    return 0;
}


static void bleprph_on_sync(void) {
  int rc;

  rc = ble_hs_util_ensure_addr(0);
  assert(rc == 0);

  /* Figure out address to use while advertising (no privacy for now) */
  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
      BLE_SERVER_LOG("error determining address type; rc=%d\n", rc);
      return;
  }

  /* Printing ADDR */
  uint8_t addr_val[6] = {0};
  rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

  BLE_SERVER_LOG("Device Address: ");
  print_addr(addr_val);
  BLE_SERVER_LOG("\n");
  /* Begin advertising. */
  bleprph_advertise();
}

void bleprph_host_task(void *param) {
  BLE_SERVER_LOG("BLE Host Task Started");
  /* This function will return only when nimble_port_stop() is executed */
  nimble_port_run();

  nimble_port_freertos_deinit();
}

static bool started = false;
void ble_server_start () {
  if (started) return;
  BLE_SERVER_LOG("START");
  started = true;
  ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
  nimble_port_init();

  ble_hs_cfg.reset_cb = bleprph_on_reset;
  ble_hs_cfg.sync_cb = bleprph_on_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 0;
  ble_hs_cfg.sm_keypress = 0;
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_oob_data_flag = 0;
  ble_hs_cfg.sm_our_key_dist = 1;
  ble_hs_cfg.sm_their_key_dist = 1;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  char device_name[] = "TEST-TCC";
  ESP_ERROR_CHECK(ble_svc_gap_device_name_set(device_name));

  ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_svr_svcs));
  ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_svr_svcs));

  /* XXX Need to have template for store */
  // ble_store_config_init();

  nimble_port_freertos_init(bleprph_host_task);
}

void ble_server_stop () {
  if (!started) return;
  BLE_SERVER_LOG("STOP");
  ble_gap_adv_stop();
  nimble_port_stop();
  nimble_port_deinit();
  esp_nimble_hci_and_controller_deinit();
  started = false;
}
