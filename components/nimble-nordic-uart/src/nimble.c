#include "nimble-nordic-uart.h"

#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <freertos/FreeRTOS.h>

static const char* _TAG = "NORDIC UART";

#define BLE_SEND_MTU 203

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define B0(x) ((x) & 0xFF)
#define B1(x) (((x) >> 8) & 0xFF)
#define B2(x) (((x) >> 16) & 0xFF)
#define B3(x) (((x) >> 24) & 0xFF)
#define B4(x) (((x) >> 32) & 0xFF)
#define B5(x) (((x) >> 40) & 0xFF)

// clang-format off
#define UUID128_CONST(a32, b16, c16, d16, e48) \
  BLE_UUID128_INIT( \
    B0(e48), B1(e48), B2(e48), B3(e48), B4(e48), B5(e48), \
    B0(d16), B1(d16), B0(c16), B1(c16), B0(b16), \
    B1(b16), B0(a32), B1(a32), B2(a32), B3(a32), \
  )
// clang-format on

static const ble_uuid128_t SERVICE_UUID = UUID128_CONST(0x6E400001, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E);
static const ble_uuid128_t CHAR_UUID_RX = UUID128_CONST(0x6E400002, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E);
static const ble_uuid128_t CHAR_UUID_TX = UUID128_CONST(0x6E400003, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E);

static uint8_t ble_addr_type;

static uint16_t ble_conn_hdl;
static uint16_t notify_char_attr_hdl;

static void (*_nordic_uart_callback)(enum nordic_uart_callback_type callback_type) = NULL;
static uart_receive_callback_t _uart_receive_callback = NULL;
static bool s_low_power_pref = false;
static bool s_adv_enabled = true;

static void _apply_conn_params(void)
{
    if (ble_conn_hdl == 0) return;
    struct ble_gap_conn_desc desc;
    int rc = ble_gap_conn_find(ble_conn_hdl, &desc);
    if (rc != 0) return;
    struct ble_gap_upd_params params;
    if (s_low_power_pref) {
        params.itvl_min = 400;
        params.itvl_max = 800;
        params.latency  = 8;
        params.supervision_timeout = 800;
    } else {
        params.itvl_min = 24;
        params.itvl_max = 40;
        params.latency  = 0;
        params.supervision_timeout = desc.supervision_timeout;
    }
    (void)ble_gap_update_params(ble_conn_hdl, &params);
}

esp_err_t nordic_uart_yield(uart_receive_callback_t uart_receive_callback) {
    _uart_receive_callback = uart_receive_callback;
    return ESP_OK;
}

static int _uart_receive(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    if (_uart_receive_callback) {
        _uart_receive_callback(ctxt);
    }
    else {
        for (int i = 0; i < ctxt->om->om_len; ++i) {
            const char c = ctxt->om->om_data[i];
            _nordic_uart_linebuf_append(c);
        }
    }
    return 0;
}

static int _uart_noop(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    return 0;
}

static const struct ble_gatt_svc_def gat_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = (ble_uuid_t*)&CHAR_UUID_RX,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .access_cb = _uart_receive,
            },
            {
                .uuid = (ble_uuid_t*)&CHAR_UUID_TX,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &notify_char_attr_hdl,
                .access_cb = _uart_noop,
            },
            { 0 },
        },
    },
    { 0 }
};

static int ble_gap_event_cb(struct ble_gap_event* event, void* arg);

static int ble_app_advertise(void) {
    if (!s_adv_enabled) {
        ESP_LOGD(_TAG, "Advertising disabled; skip start");
        return 0;
    }

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    int err = ble_gap_adv_set_fields(&fields);
    if (err) {
        ESP_LOGE(_TAG, "ble_gap_adv_set_fields, err %d", err);
        return err;
    }

    struct ble_hs_adv_fields rsp_fields;
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    
    const char* name = ble_svc_gap_device_name();
    rsp_fields.name = (uint8_t*)name;
    rsp_fields.name_len = name ? strlen(name) : 0;
    rsp_fields.name_is_complete = 1;
    
    err = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (err) {
        ESP_LOGE(_TAG, "ble_gap_adv_rsp_set_fields, err %d", err);
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 800;
    adv_params.itvl_max = 1000;

    err = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event_cb, NULL);
    if (err) {
        if (err == BLE_HS_EALREADY) {
            ESP_LOGD(_TAG, "Advertising already running");
            err = 0;
        } else {
            ESP_LOGE(_TAG, "Advertising start failed: err %d", err);
        }
    }
    return err;
}

static int ble_gap_event_cb(struct ble_gap_event* event, void* arg) {
    int rc;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(_TAG, "BLE_GAP_EVENT_CONNECT %s", event->connect.status == 0 ? "OK" : "Failed");
        if (event->connect.status == 0) {
            ble_conn_hdl = event->connect.conn_handle;
            struct ble_gap_conn_desc desc;
            int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(_TAG, "failed to find connection by handle, error code: %d", rc);
                return rc;
            }

            _apply_conn_params();
            if (_nordic_uart_callback)
                _nordic_uart_callback(NORDIC_UART_CONNECTED);
        }
        else {
            (void)ble_app_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        _nordic_uart_linebuf_append('\003');
        ESP_LOGI(_TAG, "BLE_GAP_EVENT_DISCONNECT reason=%d", event->disconnect.reason);
        ble_conn_hdl = 0;
        if (_nordic_uart_callback)
            _nordic_uart_callback(NORDIC_UART_DISCONNECTED);
        (void)ble_app_advertise();
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(_TAG, "Encryption change: status=%d", event->enc_change.status);
        return 0;

    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io pkey = {0};
        ESP_LOGI(_TAG, "Passkey action: %d", event->passkey.params.action);
        
        pkey.action = event->passkey.params.action;
        
        switch (event->passkey.params.action) {
            case BLE_SM_IOACT_NONE:
                ESP_LOGI(_TAG, "Just Works pairing");
                break;
                
            case BLE_SM_IOACT_NUMCMP:
                ESP_LOGI(_TAG, "Numeric comparison - auto accepting");
                pkey.numcmp_accept = 1;
                break;
                
            default:
                ESP_LOGW(_TAG, "Unhandled action: %d", event->passkey.params.action);
                break;
        }
        
        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        ESP_LOGI(_TAG, "ble_sm_inject_io result: %d", rc);
        return 0;
    }




    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(_TAG, "BLE_GAP_EVENT_ADV_COMPLETE");
        (void)ble_app_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == notify_char_attr_hdl) {
            if (event->subscribe.cur_notify == 0) {
                ESP_LOGI(_TAG, "Client unsubscribed from notifications");
            }
            else {
                ESP_LOGI(_TAG, "Client subscribed to notifications");
            }
        }
        else {
            ESP_LOGW(_TAG, "Unknown subscribe event for attr_handle %d", event->subscribe.attr_handle);
        }
        break;

    default:
        break;
    }
    return 0;
}

static void ble_app_on_sync_cb(void) {
    int ret = ble_hs_id_infer_auto(0, &ble_addr_type);
    if (ret != 0) {
        ESP_LOGE(_TAG, "Error ble_hs_id_infer_auto: %d", ret);
    }
    (void)ble_app_advertise();
}

static void ble_host_task(void* param) {
    ESP_LOGI(_TAG, "BLE Host Task Started");
    char* linebuf_at_start = _nordic_uart_get_linebuf();
    nimble_port_run();
    nimble_port_freertos_deinit();
    if (_nordic_uart_get_linebuf() == linebuf_at_start && linebuf_at_start != NULL) {
        _nordic_uart_buf_deinit();
    }
}

esp_err_t _nordic_uart_send(const char* message) {
    const int len = strlen(message);
    if (len == 0)
        return ESP_OK;

    for (int i = 0; i < len; i += BLE_SEND_MTU) {
        int err;
        struct os_mbuf* om;
        int err_count = 0;
    do_notify:
        om = ble_hs_mbuf_from_flat(&message[i], MIN(BLE_SEND_MTU, len - i));
        err = ble_gatts_notify_custom(ble_conn_hdl, notify_char_attr_hdl, om);
        if (err == BLE_HS_ENOMEM && err_count++ < 10) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            goto do_notify;
        }
        if (err)
            return ESP_FAIL;
    }
    return ESP_OK;
}

void nordic_uart_set_low_power_mode(bool enable)
{
    s_low_power_pref = enable;
    _apply_conn_params();
}

esp_err_t _nordic_uart_start(const char* device_name, void (*callback)(enum nordic_uart_callback_type callback_type)) {
    int rc;

    if (_nordic_uart_linebuf_initialized()) {
        ESP_LOGE(_TAG, "Already initialized");
        return ESP_FAIL;
    }

    if (nvs_flash_init() != ESP_OK) {
        ESP_LOGE(_TAG, "Failed to nvs_flash_init");
        return ESP_FAIL;
    }

    _nordic_uart_callback = callback;
    if (_nordic_uart_buf_init() != ESP_OK) {
        ESP_LOGE(_TAG, "Failed to init Nordic UART buffers");
        return ESP_FAIL;
    }
    s_adv_enabled = true;

    esp_err_t ret = nimble_port_init();    
    if (ret != ESP_OK) {
        ESP_LOGE(_TAG, "nimble_port_init() failed with error: %d", ret);
        esp_nimble_deinit();
        return ESP_FAIL;
    }

    ble_hs_cfg.sync_cb = ble_app_on_sync_cb;



    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gat_svcs);
    assert(rc == 0);

    rc = ble_gatts_add_svcs(gat_svcs);
    assert(rc == 0);

    rc = ble_svc_gap_device_name_set(device_name);
    assert(rc == 0);

    nimble_port_freertos_init(ble_host_task);

    return ESP_OK;
}

esp_err_t _nordic_uart_stop(void) {
    s_adv_enabled = false;
    if (ble_conn_hdl != 0) {
        int term_rc = ble_gap_terminate(ble_conn_hdl, BLE_ERR_REM_USER_CONN_TERM);
        if (term_rc != 0) {
            ESP_LOGW(_TAG, "ble_gap_terminate failed: %d", term_rc);
        }
        ble_conn_hdl = 0;
    }

    int rc = ble_gap_adv_stop();
    if (rc != 0) {
        if (rc == BLE_HS_EALREADY || rc == BLE_HS_EINVAL) {
            ESP_LOGD(_TAG, "Advertisement stop benign code: %d", rc);
        } else {
            ESP_LOGW(_TAG, "Error stopping advertisement: %d", rc);
        }
    }

    int ret = nimble_port_stop();
    if (ret == ESP_OK) {
        ret = nimble_port_deinit();
        if (ret != ESP_OK) {
            ESP_LOGE(_TAG, "nimble_port_deinit() failed with error: %d", ret);
            return ESP_FAIL;
        }
    }

    _nordic_uart_buf_deinit();
    _nordic_uart_callback = NULL;

    return ESP_OK;
}

esp_err_t nordic_uart_disconnect(void)
{
    if (ble_conn_hdl == 0) {
        return ESP_OK;
    }

    int rc = ble_gap_terminate(ble_conn_hdl, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        if (rc == BLE_HS_EALREADY || rc == BLE_HS_ENOTCONN) {
            ESP_LOGD(_TAG, "Disconnect benign code: %d", rc);
            return ESP_OK;
        }
        ESP_LOGW(_TAG, "ble_gap_terminate failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t nordic_uart_set_advertising_enabled(bool enable)
{
    s_adv_enabled = enable;
    if (enable) {
        int rc = ble_app_advertise();
        return (rc == 0) ? ESP_OK : ESP_FAIL;
    }

    int rc = ble_gap_adv_stop();
    if (rc != 0) {
        if (rc == BLE_HS_EALREADY || rc == BLE_HS_EINVAL || rc == BLE_HS_EBUSY) {
            ESP_LOGD(_TAG, "Advertisement stop benign code: %d", rc);
            return ESP_OK;
        } else {
            ESP_LOGW(_TAG, "Error stopping advertisement: %d", rc);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}