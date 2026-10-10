#include "direct_connection.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BLE_JSON_SIZE 1024
#define BLE_FRAME_HEADER_SIZE 4

static const char *TAG = "BLE_MANAGER";
static ble_manager_t *active_manager;
static uint8_t own_address_type;
static uint16_t device_info_handle;
static uint16_t telemetry_handle;
static uint16_t config_handle;
static uint16_t command_handle;
static uint16_t next_message_id = 1;
void ble_store_config_init(void);

static const ble_uuid128_t service_uuid = BLE_UUID128_INIT(
    0x01, 0x00, 0x00, 0x00, 0x2c, 0x9a, 0x3b, 0x9e,
    0x70, 0x4d, 0x7a, 0x6c, 0x01, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t device_info_uuid = BLE_UUID128_INIT(
    0x01, 0x00, 0x00, 0x00, 0x2c, 0x9a, 0x3b, 0x9e,
    0x70, 0x4d, 0x7a, 0x6c, 0x02, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t telemetry_uuid = BLE_UUID128_INIT(
    0x01, 0x00, 0x00, 0x00, 0x2c, 0x9a, 0x3b, 0x9e,
    0x70, 0x4d, 0x7a, 0x6c, 0x03, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t config_uuid = BLE_UUID128_INIT(
    0x01, 0x00, 0x00, 0x00, 0x2c, 0x9a, 0x3b, 0x9e,
    0x70, 0x4d, 0x7a, 0x6c, 0x04, 0x00, 0xf0, 0xa0);
static const ble_uuid128_t command_uuid = BLE_UUID128_INIT(
    0x01, 0x00, 0x00, 0x00, 0x2c, 0x9a, 0x3b, 0x9e,
    0x70, 0x4d, 0x7a, 0x6c, 0x05, 0x00, 0xf0, 0xa0);

static int append_provider_value(
    struct ble_gatt_access_ctxt *ctxt, direct_json_provider_t provider)
{
    if (active_manager == NULL || provider == NULL) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    char value[BLE_JSON_SIZE];
    esp_err_t result = provider(active_manager->callbacks.context,
                               value,
                               sizeof(value));
    if (result != ESP_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return os_mbuf_append(ctxt->om, value, strlen(value)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int consume_value(
    struct ble_gatt_access_ctxt *ctxt, direct_json_consumer_t consumer)
{
    if (active_manager == NULL || consumer == NULL) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }
    char value[BLE_JSON_SIZE];
    uint16_t length = 0;
    if (OS_MBUF_PKTLEN(ctxt->om) >= sizeof(value) ||
        ble_hs_mbuf_to_flat(
            ctxt->om, value, sizeof(value) - 1, &length) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    value[length] = '\0';
    esp_err_t result = consumer(active_manager->callbacks.context, value, length);
    ESP_LOGI(TAG, "GATT write processed: bytes=%u result=%s stack_hwm=%u",
             (unsigned)length, esp_err_to_name(result),
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    return result == ESP_OK ? 0 : BLE_ATT_ERR_UNLIKELY;
}

static int gatt_access(
    uint16_t connection_handle,
    uint16_t attribute_handle,
    struct ble_gatt_access_ctxt *ctxt,
    void *arg)
{
    (void)connection_handle;
    (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (attribute_handle == device_info_handle) {
            return append_provider_value(
                ctxt, active_manager->callbacks.device_info);
        }
        if (attribute_handle == telemetry_handle) {
            return append_provider_value(
                ctxt, active_manager->callbacks.latest_telemetry);
        }
        if (attribute_handle == config_handle) {
            return append_provider_value(
                ctxt, active_manager->callbacks.get_config);
        }
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (attribute_handle == config_handle) {
            return consume_value(ctxt, active_manager->callbacks.set_config);
        }
        if (attribute_handle == command_handle) {
            return consume_value(ctxt, active_manager->callbacks.command);
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &device_info_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_READ,
                .val_handle = &device_info_handle,
            },
            {
                .uuid = &telemetry_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &telemetry_handle,
            },
            {
                .uuid = &config_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_WRITE_ENC,
                .val_handle = &config_handle,
            },
            {
                .uuid = &command_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .val_handle = &command_handle,
            },
            {0},
        },
    },
    {0},
};

static int gap_event(struct ble_gap_event *event, void *arg);

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int result = ble_gap_adv_set_fields(&fields);
    if (result != 0) {
        ESP_LOGE(TAG, "Set advertising data failed: %d", result);
        return;
    }

    /* Keep the 128-bit service UUID in the 31-byte advertising packet and
     * move the human-readable name to the scan response packet. */
    struct ble_hs_adv_fields response = {0};
    const char *name = ble_svc_gap_device_name();
    response.name = (uint8_t *)name;
    response.name_len = strlen(name);
    response.name_is_complete = 1;
    result = ble_gap_adv_rsp_set_fields(&response);
    if (result != 0) {
        ESP_LOGE(TAG, "Set scan response failed: %d", result);
        return;
    }

    struct ble_gap_adv_params parameters = {0};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    result = ble_gap_adv_start(own_address_type,
                               NULL,
                               BLE_HS_FOREVER,
                               &parameters,
                               gap_event,
                               NULL);
    if (result != 0) {
        ESP_LOGE(TAG, "Start advertising failed: %d", result);
    }
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (active_manager == NULL) {
        return 0;
    }
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            active_manager->connected = true;
            active_manager->connection_handle = event->connect.conn_handle;
            ESP_LOGI(TAG,
                     "BLE connected: handle=%u",
                     (unsigned)event->connect.conn_handle);
        } else {
            advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        active_manager->connected = false;
        active_manager->subscribed = false;
        active_manager->connection_handle = BLE_HS_CONN_HANDLE_NONE;
        ESP_LOGI(TAG, "BLE disconnected; advertising resumed");
        advertise();
        return 0;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == telemetry_handle) {
            active_manager->subscribed = event->subscribe.cur_notify != 0;
            ESP_LOGI(TAG,
                     "Telemetry notify subscription=%d",
                     active_manager->subscribed);
        }
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc connection;
        int result = ble_gap_conn_find(event->enc_change.conn_handle, &connection);
        ESP_LOGI(TAG, "BLE encryption status=%d encrypted=%d bonded=%d",
                 event->enc_change.status,
                 result == 0 && connection.sec_state.encrypted,
                 result == 0 && connection.sec_state.bonded);
        return 0;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        ESP_LOGW(TAG, "Peer requested repeat pairing; existing bond preserved");
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    default:
        return 0;
    }
}

static void on_sync(void)
{
    int result = ble_hs_util_ensure_addr(0);
    if (result == 0) {
        result = ble_hs_id_infer_auto(0, &own_address_type);
    }
    if (result != 0) {
        ESP_LOGE(TAG, "BLE identity setup failed: %d", result);
        return;
    }
    advertise();
}

static void host_task(void *parameter)
{
    (void)parameter;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_manager_start(
    ble_manager_t *manager,
    const char *device_name,
    const direct_connection_callbacks_t *callbacks)
{
    if (manager == NULL || device_name == NULL || callbacks == NULL ||
        active_manager != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(manager, 0, sizeof(*manager));
    manager->connection_handle = BLE_HS_CONN_HANDLE_NONE;
    manager->callbacks = *callbacks;
    active_manager = manager;

    esp_err_t result = nimble_port_init();
    if (result != ESP_OK) {
        active_manager = NULL;
        return result;
    }
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(services);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(services);
    }
    if (rc == 0) {
        rc = ble_svc_gap_device_name_set(device_name);
    }
    if (rc != 0) {
        active_manager = NULL;
        return ESP_FAIL;
    }
    manager->telemetry_value_handle = telemetry_handle;
    manager->initialized = true;
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "BLE GATT ready: name=%s", device_name);
    return ESP_OK;
}

esp_err_t ble_manager_notify_telemetry(
    ble_manager_t *manager, const char *payload)
{
    if (manager == NULL || payload == NULL || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!manager->connected || !manager->subscribed) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t payload_length = strlen(payload);
    uint16_t mtu = ble_att_mtu(manager->connection_handle);
    size_t chunk_size = mtu > BLE_FRAME_HEADER_SIZE + 3
                            ? mtu - BLE_FRAME_HEADER_SIZE - 3
                            : 16;
    size_t frame_count =
        (payload_length + chunk_size - 1U) / chunk_size;
    if (frame_count == 0 || frame_count > UINT8_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint16_t message_id = next_message_id++;
    for (size_t index = 0; index < frame_count; index++) {
        size_t offset = index * chunk_size;
        size_t remaining = payload_length - offset;
        size_t bytes = remaining < chunk_size ? remaining : chunk_size;
        struct os_mbuf *packet = os_msys_get_pkthdr(
            BLE_FRAME_HEADER_SIZE + bytes, 0);
        if (packet == NULL) {
            return ESP_ERR_NO_MEM;
        }
        uint8_t header[BLE_FRAME_HEADER_SIZE] = {
            (uint8_t)(message_id >> 8),
            (uint8_t)(message_id & 0xff),
            (uint8_t)index,
            (uint8_t)frame_count,
        };
        if (os_mbuf_append(packet, header, sizeof(header)) != 0 ||
            os_mbuf_append(packet, payload + offset, bytes) != 0) {
            os_mbuf_free_chain(packet);
            return ESP_ERR_NO_MEM;
        }
        int result = ble_gatts_notify_custom(manager->connection_handle,
                                             telemetry_handle,
                                             packet);
        if (result != 0) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

bool ble_manager_is_connected(ble_manager_t *manager)
{
    return manager != NULL && manager->initialized && manager->connected;
}
