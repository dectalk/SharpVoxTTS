#include "ble_spp.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"

// NimBLE stack
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_spp";

static const ble_uuid128_t nus_svc_uuid =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);

static const ble_uuid128_t nus_chr_rx_uuid =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E);

static const ble_uuid128_t nus_chr_tx_uuid =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E);

static ble_spp_recv_cb_t s_recv_cb = NULL;
static uint16_t s_nus_tx_handle;
static uint8_t s_addr_type;

#define BLE_PKT_MAX  512   // max single BLE ATT packet (MTU ceiling)
#define BLE_LINE_MAX 4096  // max accumulated line length
static char   s_line_buf[BLE_LINE_MAX];
static size_t s_line_len = 0;

static int ble_spp_gap_event(struct ble_gap_event *event, void *arg);

static void ble_spp_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &nus_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    struct ble_hs_adv_fields rsp_fields;
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    rsp_fields.name = (uint8_t *)"SharpVox";
    rsp_fields.name_len = strlen("SharpVox");
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting scan response data; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_spp_gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started as 'SharpVox'");
}

static int nus_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ble_uuid_cmp(ctxt->chr->uuid, &nus_chr_rx_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t pkt_len = OS_MBUF_PKTLEN(ctxt->om);
            if (pkt_len > 0) {
                char chunk[BLE_PKT_MAX];
                if (pkt_len > BLE_PKT_MAX) pkt_len = BLE_PKT_MAX;
                ble_hs_mbuf_to_flat(ctxt->om, chunk, pkt_len, NULL);
                for (uint16_t i = 0; i < pkt_len; i++) {
                    char c = chunk[i];
                    if (c == '\n') {
                        s_line_buf[s_line_len] = '\0';
                        if (s_line_len > 0) {
                            ESP_LOGI(TAG, "RX line (%d bytes): %.80s…", (int)s_line_len, s_line_buf);
                            if (s_recv_cb) s_recv_cb(s_line_buf);
                        }
                        s_line_len = 0;
                    } else if (c != '\r') {
                        if (s_line_len < BLE_LINE_MAX - 1)
                            s_line_buf[s_line_len++] = c;
                    }
                }
            }
            return 0;
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            /* Some apps try to read the RX char; return nothing */
            return 0;
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def nus_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &nus_chr_rx_uuid.u,
                .access_cb = nus_gatt_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = &nus_chr_tx_uuid.u,
                .access_cb = nus_gatt_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_nus_tx_handle,
            },
            { 0 }
        },
    },
    { 0 }
};

static int ble_spp_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0) {
                ESP_LOGI(TAG, "Remote Addr: %02X:%02X:%02X:%02X:%02X:%02X",
                         desc.peer_id_addr.val[5], desc.peer_id_addr.val[4],
                         desc.peer_id_addr.val[3], desc.peer_id_addr.val[2],
                         desc.peer_id_addr.val[1], desc.peer_id_addr.val[0]);
            }
        } else {
            ble_spp_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
        ble_spp_advertise();
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "Connection updated; status=%d", event->conn_update.status);
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU updated; conn_handle=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.value);
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertise complete");
        ble_spp_advertise();
        break;
    }
    return 0;
}

static void ble_spp_on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset; reason=%d — re-sync pending", reason);
}

static void ble_spp_on_sync(void) {
    int rc;
    rc = ble_hs_id_infer_auto(0, &s_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return;
    }
    ble_spp_advertise();
}

static void ble_spp_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_spp_init(ble_spp_recv_cb_t recv_cb) {
    int rc;
    s_recv_cb = recv_cb;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    rc = nimble_port_init();
    if (rc != 0) return ESP_FAIL;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(nus_gatt_svcs);
    if (rc != 0) return ESP_FAIL;

    rc = ble_gatts_add_svcs(nus_gatt_svcs);
    if (rc != 0) return ESP_FAIL;

    rc = ble_svc_gap_device_name_set("SharpVox");
    assert(rc == 0);

    ble_svc_gap_device_appearance_set(192);

    ble_hs_cfg.sync_cb        = ble_spp_on_sync;
    ble_hs_cfg.reset_cb       = ble_spp_on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // saves LTK to NVS so the phone can reconnect without forgetting the device.
    ble_hs_cfg.sm_io_cap         = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_sc             = 1;
    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

    nimble_port_freertos_init(ble_spp_host_task);
    return ESP_OK;
}
