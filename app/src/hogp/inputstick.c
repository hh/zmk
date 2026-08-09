/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * InputStick client -- drive an InputStick USB HID dongle over BLE.
 *
 * The dongle is NOT a HID peripheral, so none of the HOGP machinery applies.
 * It exposes a Nordic UART Service and speaks a framed binary protocol on top
 * of it. Byte layouts here are ported from the hardware-verified prototypes in
 * /var/srv/media/inputstick_ble.py and inputstick/public/ble-protocol.js --
 * treat those as ground truth if anything here disagrees with the wire.
 *
 * Wire format
 * -----------
 *   packet  = [0x55][flags][block]*n
 *   flags   = n_blocks (low 6 bits) | 0x80 respond | 0x40 encrypted | 0x20 hmac
 *   payload = [CRC32-BE 4B][cmd 1B][param 1B][data...] zero-padded to 16B
 *   CRC32 covers everything from offset 4 onward, including the padding.
 *
 * The header and each 16-byte block go out as SEPARATE GATT writes, and the
 * whole sequence must be atomic -- interleaving two packets' writes corrupts
 * both and the dongle drops them silently (HID data packets ask for no
 * response, so nothing surfaces an error). Hence packet_mutex.
 *
 * Writes are acknowledged (bt_gatt_write, an ATT Write Request) rather than
 * write-without-response. The prototypes found that unacked writes drop
 * silently, and a dropped "all keys released" report leaves a key
 * auto-repeating on the target machine forever.
 *
 * Coexistence: this shares the BLE scanner with hogp_central.c. Don't run
 * !istick and !pair at the same time -- whichever stops the scan last wins.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <string.h>

#include <zmk/hogp/inputstick.h>

LOG_MODULE_REGISTER(inputstick, CONFIG_ZMK_INPUTSTICK_LOG_LEVEL);

/* ── protocol constants ──────────────────────────────────────────────────── */

#define IS_START_TAG 0x55
#define IS_FLAG_RESPOND 0x80
#define IS_FLAG_ENCRYPTED 0x40
#define IS_FLAG_HMAC 0x20
#define IS_BLOCK_SIZE 16
#define IS_CRC_OFFSET 4

#define IS_CMD_RUN_FW 0x04
#define IS_CMD_FW_INFO 0x10
#define IS_CMD_INIT 0x11
#define IS_CMD_SET_UPDATE_INTERVAL 0x31
#define IS_CMD_HID_DATA_KEYB 0x21
#define IS_CMD_HID_DATA_MOUSE 0x23
#define IS_CMD_HID_STATUS 0x2f

#define IS_MOD_LSHIFT 0x02

/* Largest payload we build: cmd+param+3 keyboard reports (24B) + CRC = 30 -> 32 */
#define IS_MAX_PAYLOAD 48
#define IS_MAX_RESP 32
#define IS_RX_BUF_SIZE 128

/* Nordic UART Service, and the legacy HM-10/CC2540 service older dongles use. */
static const struct bt_uuid_128 nus_service_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e));
static const struct bt_uuid_128 nus_write_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e));
static const struct bt_uuid_128 nus_notify_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x6e400003, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e));

static const struct bt_uuid_16 hm_service_uuid = BT_UUID_INIT_16(0xffe0);
static const struct bt_uuid_16 hm_rxtx_uuid = BT_UUID_INIT_16(0xffe1);

/* ── state ───────────────────────────────────────────────────────────────── */

enum is_state {
    IS_STATE_IDLE,
    IS_STATE_SCANNING,
    IS_STATE_CONNECTING,
    IS_STATE_DISCOVERING,
    IS_STATE_HANDSHAKING,
    IS_STATE_READY,
};

enum is_discover_phase {
    IS_DISC_SERVICE,
    IS_DISC_CHARACTERISTICS,
};

static struct {
    enum is_state state;
    struct bt_conn *conn;
    bt_addr_le_t addr;

    /* Which service flavour this dongle speaks; drives the UUIDs we look for. */
    bool legacy_hm;
    enum is_discover_phase discover_phase;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t write_handle;
    uint16_t notify_handle;

    uint16_t fw_version;
    bool got_status;

    struct bt_gatt_discover_params discover_params;
    struct bt_gatt_discover_params sub_discover_params;
    struct bt_gatt_subscribe_params subscribe_params;
    struct bt_gatt_write_params write_params;

    uint8_t rx_buf[IS_RX_BUF_SIZE];
    size_t rx_len;

    uint8_t resp[IS_MAX_RESP];
    size_t resp_len;
} istick;

static bool scan_running;

/* ── target slots ─────────────────────────────────────────────────────────── */

struct is_slot {
    bt_addr_le_t addr;
    bool bound;
};

static struct is_slot slots[CONFIG_ZMK_INPUTSTICK_SLOT_COUNT];
static uint8_t active_slot;

static void is_slot_save(uint8_t slot) {
    char key[32];
    snprintk(key, sizeof(key), "inputstick/slot_%u", slot);
    if (slots[slot].bound) {
        settings_save_one(key, &slots[slot].addr, sizeof(slots[slot].addr));
    } else {
        settings_delete(key);
    }
}

static int is_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (strncmp(name, "slot_", 5) == 0) {
        unsigned long slot = strtoul(name + 5, NULL, 10);
        if (slot >= CONFIG_ZMK_INPUTSTICK_SLOT_COUNT || len != sizeof(bt_addr_le_t)) {
            return -EINVAL;
        }
        if (read_cb(cb_arg, &slots[slot].addr, sizeof(bt_addr_le_t)) > 0) {
            slots[slot].bound = true;
            LOG_INF("slot %lu restored from settings", slot);
        }
        return 0;
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(inputstick, "inputstick", NULL, is_settings_set, NULL, NULL);

uint8_t inputstick_active_slot(void) { return active_slot; }

bool inputstick_slot_is_bound(uint8_t slot) {
    return slot < CONFIG_ZMK_INPUTSTICK_SLOT_COUNT && slots[slot].bound;
}

static K_SEM_DEFINE(write_done_sem, 0, 1);
static K_SEM_DEFINE(resp_sem, 0, 1);
static K_SEM_DEFINE(handshake_sem, 0, 1);
static K_MUTEX_DEFINE(packet_mutex);

static int is_write_err;

/* ── ASCII -> USB HID keycode (US layout) ────────────────────────────────── */

/*
 * Deliberately a superset of the JS prototype's CHAR_MAP, which has no entries
 * for shifted digits (!@#$%^&*) and maps '#' to the non-US-hash key 0x32. On a
 * US layout those are shift+digit, so a typing test using the JS table would
 * silently skip them. Fixed here; if a dongle ever needs the non-US behaviour
 * that belongs behind a layout option, not baked into the table.
 */
static bool ascii_to_hid(char c, uint8_t *mod, uint8_t *key) {
    *mod = 0;
    *key = 0;

    if (c >= 'a' && c <= 'z') {
        *key = 0x04 + (c - 'a');
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        *mod = IS_MOD_LSHIFT;
        *key = 0x04 + (c - 'A');
        return true;
    }
    if (c >= '1' && c <= '9') {
        *key = 0x1e + (c - '1');
        return true;
    }

    switch (c) {
    case '0': *key = 0x27; return true;
    case '!': *mod = IS_MOD_LSHIFT; *key = 0x1e; return true;
    case '@': *mod = IS_MOD_LSHIFT; *key = 0x1f; return true;
    case '#': *mod = IS_MOD_LSHIFT; *key = 0x20; return true;
    case '$': *mod = IS_MOD_LSHIFT; *key = 0x21; return true;
    case '%': *mod = IS_MOD_LSHIFT; *key = 0x22; return true;
    case '^': *mod = IS_MOD_LSHIFT; *key = 0x23; return true;
    case '&': *mod = IS_MOD_LSHIFT; *key = 0x24; return true;
    case '*': *mod = IS_MOD_LSHIFT; *key = 0x25; return true;
    case '(': *mod = IS_MOD_LSHIFT; *key = 0x26; return true;
    case ')': *mod = IS_MOD_LSHIFT; *key = 0x27; return true;
    case '\n': *key = 0x28; return true;
    case '\t': *key = 0x2b; return true;
    case ' ': *key = 0x2c; return true;
    case '-': *key = 0x2d; return true;
    case '_': *mod = IS_MOD_LSHIFT; *key = 0x2d; return true;
    case '=': *key = 0x2e; return true;
    case '+': *mod = IS_MOD_LSHIFT; *key = 0x2e; return true;
    case '[': *key = 0x2f; return true;
    case '{': *mod = IS_MOD_LSHIFT; *key = 0x2f; return true;
    case ']': *key = 0x30; return true;
    case '}': *mod = IS_MOD_LSHIFT; *key = 0x30; return true;
    case '\\': *key = 0x31; return true;
    case '|': *mod = IS_MOD_LSHIFT; *key = 0x31; return true;
    case ';': *key = 0x33; return true;
    case ':': *mod = IS_MOD_LSHIFT; *key = 0x33; return true;
    case '\'': *key = 0x34; return true;
    case '"': *mod = IS_MOD_LSHIFT; *key = 0x34; return true;
    case '`': *key = 0x35; return true;
    case '~': *mod = IS_MOD_LSHIFT; *key = 0x35; return true;
    case ',': *key = 0x36; return true;
    case '<': *mod = IS_MOD_LSHIFT; *key = 0x36; return true;
    case '.': *key = 0x37; return true;
    case '>': *mod = IS_MOD_LSHIFT; *key = 0x37; return true;
    case '/': *key = 0x38; return true;
    case '?': *mod = IS_MOD_LSHIFT; *key = 0x38; return true;
    default: return false;
    }
}

/* ── packet TX ───────────────────────────────────────────────────────────── */

static void is_write_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params) {
    is_write_err = err;
    k_sem_give(&write_done_sem);
}

/* One acknowledged GATT write. Caller must hold packet_mutex. */
static int is_write_raw(const uint8_t *data, uint16_t len) {
    if (!istick.conn || !istick.write_handle) {
        return -ENOTCONN;
    }

    memset(&istick.write_params, 0, sizeof(istick.write_params));
    istick.write_params.func = is_write_cb;
    istick.write_params.handle = istick.write_handle;
    istick.write_params.offset = 0;
    istick.write_params.data = data;
    istick.write_params.length = len;

    k_sem_reset(&write_done_sem);
    int err = bt_gatt_write(istick.conn, &istick.write_params);
    if (err) {
        LOG_ERR("bt_gatt_write failed (err %d)", err);
        return err;
    }

    if (k_sem_take(&write_done_sem, K_SECONDS(2)) != 0) {
        LOG_ERR("GATT write timed out");
        return -ETIMEDOUT;
    }
    if (is_write_err) {
        LOG_ERR("GATT write rejected (att err 0x%02x)", is_write_err);
        return -EIO;
    }
    return 0;
}

/*
 * Build and send one framed packet. If respond is true, block until the dongle
 * notifies a reply (or timeout) and copy it into resp/resp_len.
 */
static int is_send_packet(uint8_t cmd, uint8_t param, const uint8_t *data, size_t data_len,
                          bool respond, uint8_t *resp_out, size_t *resp_out_len) {
    uint8_t payload[IS_MAX_PAYLOAD];

    size_t inner_len = IS_CRC_OFFSET + 2 + data_len;
    size_t padded = ROUND_UP(inner_len, IS_BLOCK_SIZE);
    if (padded > sizeof(payload)) {
        LOG_ERR("payload too big (%zu > %zu)", padded, sizeof(payload));
        return -EINVAL;
    }

    memset(payload, 0, padded);
    payload[IS_CRC_OFFSET] = cmd;
    payload[IS_CRC_OFFSET + 1] = param;
    if (data_len) {
        memcpy(&payload[IS_CRC_OFFSET + 2], data, data_len);
    }

    /* CRC32 over cmd+param+data+padding, stored big-endian at offset 0. */
    uint32_t crc = crc32_ieee(&payload[IS_CRC_OFFSET], padded - IS_CRC_OFFSET);
    sys_put_be32(crc, payload);

    uint8_t n_blocks = padded / IS_BLOCK_SIZE;
    uint8_t header[2] = {IS_START_TAG, n_blocks & 0x3f};
    if (respond) {
        header[1] |= IS_FLAG_RESPOND;
    }

    k_mutex_lock(&packet_mutex, K_FOREVER);

    if (respond) {
        k_sem_reset(&resp_sem);
    }

    int err = is_write_raw(header, sizeof(header));
    for (uint8_t i = 0; err == 0 && i < n_blocks; i++) {
        err = is_write_raw(&payload[i * IS_BLOCK_SIZE], IS_BLOCK_SIZE);
    }

    if (err == 0 && respond) {
        if (k_sem_take(&resp_sem, K_SECONDS(5)) != 0) {
            LOG_WRN("no response to cmd 0x%02x", cmd);
            err = -ETIMEDOUT;
        } else if (resp_out && resp_out_len) {
            size_t n = MIN(istick.resp_len, IS_MAX_RESP);
            memcpy(resp_out, istick.resp, n);
            *resp_out_len = n;
        }
    }

    k_mutex_unlock(&packet_mutex);
    return err;
}

/* ── packet RX ───────────────────────────────────────────────────────────── */

static void is_handle_body(const uint8_t *body, size_t len) {
    if (len < 2) {
        return;
    }

    if (body[0] == IS_CMD_HID_STATUS) {
        istick.got_status = true;
        LOG_DBG("HID_STATUS received");
        return;
    }

    istick.resp_len = MIN(len, (size_t)IS_MAX_RESP);
    memcpy(istick.resp, body, istick.resp_len);
    k_sem_give(&resp_sem);
}

static void is_drain_packets(void) {
    for (;;) {
        if (istick.rx_len < 2) {
            return;
        }

        if (istick.rx_buf[0] != IS_START_TAG) {
            LOG_WRN("bad start tag 0x%02x, resyncing", istick.rx_buf[0]);
            memmove(istick.rx_buf, istick.rx_buf + 1, --istick.rx_len);
            continue;
        }

        uint8_t flags = istick.rx_buf[1];
        uint8_t n_blocks = flags & 0x3f;
        bool encrypted = flags & IS_FLAG_ENCRYPTED;
        bool has_hmac = flags & IS_FLAG_HMAC;
        size_t total = 2 + (size_t)n_blocks * IS_BLOCK_SIZE + (has_hmac ? 20 : 0);

        if (total > sizeof(istick.rx_buf)) {
            LOG_ERR("packet claims %zu bytes, dropping stream", total);
            istick.rx_len = 0;
            return;
        }
        if (istick.rx_len < total) {
            return; /* wait for more notifications */
        }

        const uint8_t *payload = &istick.rx_buf[2];
        size_t payload_len = (size_t)n_blocks * IS_BLOCK_SIZE;

        if (payload_len >= IS_CRC_OFFSET + 2) {
            if (!encrypted) {
                uint32_t expected = sys_get_be32(payload);
                uint32_t actual =
                    crc32_ieee(&payload[IS_CRC_OFFSET], payload_len - IS_CRC_OFFSET);
                if (expected != actual) {
                    LOG_WRN("CRC mismatch: expected %08x got %08x", expected, actual);
                }
            }
            is_handle_body(&payload[IS_CRC_OFFSET], payload_len - IS_CRC_OFFSET);
        }

        istick.rx_len -= total;
        memmove(istick.rx_buf, istick.rx_buf + total, istick.rx_len);
    }
}

static uint8_t is_notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                            const void *data, uint16_t length) {
    if (!data) {
        LOG_INF("notifications stopped");
        return BT_GATT_ITER_STOP;
    }

    if (istick.rx_len + length > sizeof(istick.rx_buf)) {
        LOG_WRN("rx buffer overflow, resetting");
        istick.rx_len = 0;
    }

    memcpy(&istick.rx_buf[istick.rx_len], data, length);
    istick.rx_len += length;
    is_drain_packets();

    return BT_GATT_ITER_CONTINUE;
}

/* ── handshake ───────────────────────────────────────────────────────────── */

/*
 * Runs on its own thread: the sequence blocks waiting for GATT writes and
 * notifications, which must never happen on the BT RX thread or a workqueue.
 */
static void is_handshake(void) {
    uint8_t resp[IS_MAX_RESP];
    size_t resp_len = 0;

    LOG_INF("InputStick handshake starting...");
    istick.state = IS_STATE_HANDSHAKING;
    istick.fw_version = 0;
    istick.got_status = false;

    /* RUN_FW: the dongle may be sitting in its bootloader; retry once. */
    int err = is_send_packet(IS_CMD_RUN_FW, 0, NULL, 0, true, resp, &resp_len);
    if (err) {
        k_msleep(1000);
        err = is_send_packet(IS_CMD_RUN_FW, 0, NULL, 0, true, resp, &resp_len);
        if (err) {
            LOG_ERR("RUN_FW got no answer -- dongle not responding");
            goto fail;
        }
    }

    resp_len = 0;
    err = is_send_packet(IS_CMD_FW_INFO, 0, NULL, 0, true, resp, &resp_len);
    if (err == 0 && resp_len >= 4 && resp[0] == IS_CMD_FW_INFO) {
        istick.fw_version = ((uint16_t)resp[2] << 8) | resp[3];
        LOG_INF("dongle firmware version %d", istick.fw_version);
    } else {
        LOG_WRN("FW_INFO did not return a version");
    }

    err = is_send_packet(IS_CMD_INIT, 0, NULL, 0, true, NULL, NULL);
    if (err) {
        LOG_ERR("INIT failed (err %d)", err);
        goto fail;
    }

    if (istick.fw_version >= 100) {
        is_send_packet(IS_CMD_SET_UPDATE_INTERVAL, 5, NULL, 0, true, NULL, NULL);
    }

    /* Wait for the dongle's first HID_STATUS -- proof the HID side came up. */
    for (int i = 0; i < 50 && !istick.got_status; i++) {
        k_msleep(100);
    }

    /*
     * Fail loudly rather than reporting a connection that cannot type. The JS
     * prototype used to mark itself connected here unconditionally and every
     * later command failed with a swallowed error.
     */
    if (istick.fw_version == 0 || !istick.got_status) {
        LOG_ERR("handshake incomplete (fw=%d status=%d) -- not usable", istick.fw_version,
                istick.got_status);
        goto fail;
    }

    istick.state = IS_STATE_READY;
    LOG_INF("=== InputStick READY (fw %d) ===", istick.fw_version);
    return;

fail:
    if (istick.conn) {
        bt_conn_disconnect(istick.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

static void is_thread_fn(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (;;) {
        k_sem_take(&handshake_sem, K_FOREVER);
        is_handshake();
    }
}

K_THREAD_DEFINE(inputstick_tid, CONFIG_ZMK_INPUTSTICK_THREAD_STACK_SIZE, is_thread_fn, NULL, NULL,
                NULL, 10, 0, 0);

/* ── GATT discovery ──────────────────────────────────────────────────────── */

static void is_subscribe(void) {
    memset(&istick.subscribe_params, 0, sizeof(istick.subscribe_params));
    istick.subscribe_params.notify = is_notify_cb;
    istick.subscribe_params.value = BT_GATT_CCC_NOTIFY;
    istick.subscribe_params.value_handle = istick.notify_handle;
    istick.subscribe_params.ccc_handle = 0; /* auto-discovered, see BT_GATT_AUTO_DISCOVER_CCC */
    istick.subscribe_params.disc_params = &istick.sub_discover_params;
    istick.subscribe_params.end_handle = istick.service_end_handle;

    int err = bt_gatt_subscribe(istick.conn, &istick.subscribe_params);
    if (err && err != -EALREADY) {
        LOG_ERR("subscribe failed (err %d)", err);
        bt_conn_disconnect(istick.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    LOG_INF("subscribed to notify handle 0x%04x, starting handshake", istick.notify_handle);
    k_sem_give(&handshake_sem);
}

static void is_discover_characteristics(void);

static uint8_t is_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              struct bt_gatt_discover_params *params) {
    if (!attr) {
        if (istick.discover_phase == IS_DISC_SERVICE) {
            if (!istick.legacy_hm) {
                /* No NUS -- retry as an older HM-10/CC2540 dongle. */
                LOG_INF("no Nordic UART service, retrying as legacy HM-10");
                istick.legacy_hm = true;
                memset(&istick.discover_params, 0, sizeof(istick.discover_params));
                istick.discover_params.uuid = &hm_service_uuid.uuid;
                istick.discover_params.func = is_discover_cb;
                istick.discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
                istick.discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
                istick.discover_params.type = BT_GATT_DISCOVER_PRIMARY;
                if (bt_gatt_discover(conn, &istick.discover_params) == 0) {
                    return BT_GATT_ITER_STOP;
                }
            }
            LOG_ERR("no InputStick service found -- not an InputStick dongle");
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            return BT_GATT_ITER_STOP;
        }

        /* Characteristic phase finished. */
        if (istick.write_handle && istick.notify_handle) {
            is_subscribe();
        } else {
            LOG_ERR("missing characteristics (write=0x%04x notify=0x%04x)", istick.write_handle,
                    istick.notify_handle);
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
        return BT_GATT_ITER_STOP;
    }

    if (istick.discover_phase == IS_DISC_SERVICE) {
        struct bt_gatt_service_val *service = attr->user_data;
        istick.service_start_handle = attr->handle;
        istick.service_end_handle = service->end_handle;
        LOG_INF("found %s service: handles 0x%04x-0x%04x", istick.legacy_hm ? "HM-10" : "NUS",
                istick.service_start_handle, istick.service_end_handle);

        /*
         * Stop here and restart discovery for characteristics rather than
         * returning CONTINUE -- same reasoning as hogp_central.c, where
         * continuing races incoming ATT traffic from the peer.
         */
        is_discover_characteristics();
        return BT_GATT_ITER_STOP;
    }

    /* Characteristic phase. */
    struct bt_gatt_chrc *chrc = attr->user_data;
    const struct bt_uuid *want_write = istick.legacy_hm ? &hm_rxtx_uuid.uuid : &nus_write_uuid.uuid;
    const struct bt_uuid *want_notify =
        istick.legacy_hm ? &hm_rxtx_uuid.uuid : &nus_notify_uuid.uuid;

    if (!bt_uuid_cmp(chrc->uuid, want_write) && (chrc->properties & BT_GATT_CHRC_WRITE)) {
        istick.write_handle = chrc->value_handle;
        LOG_INF("write characteristic at 0x%04x", istick.write_handle);
    }
    if (!bt_uuid_cmp(chrc->uuid, want_notify) && (chrc->properties & BT_GATT_CHRC_NOTIFY)) {
        istick.notify_handle = chrc->value_handle;
        LOG_INF("notify characteristic at 0x%04x", istick.notify_handle);
    }

    return BT_GATT_ITER_CONTINUE;
}

static void is_discover_characteristics(void) {
    istick.discover_phase = IS_DISC_CHARACTERISTICS;
    memset(&istick.discover_params, 0, sizeof(istick.discover_params));
    istick.discover_params.uuid = NULL; /* all characteristics in the service */
    istick.discover_params.func = is_discover_cb;
    istick.discover_params.start_handle = istick.service_start_handle;
    istick.discover_params.end_handle = istick.service_end_handle;
    istick.discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

    int err = bt_gatt_discover(istick.conn, &istick.discover_params);
    if (err) {
        LOG_ERR("characteristic discovery failed (err %d)", err);
        bt_conn_disconnect(istick.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

static void is_start_discovery(void) {
    istick.state = IS_STATE_DISCOVERING;
    istick.legacy_hm = false;
    istick.discover_phase = IS_DISC_SERVICE;
    istick.write_handle = 0;
    istick.notify_handle = 0;

    memset(&istick.discover_params, 0, sizeof(istick.discover_params));
    istick.discover_params.uuid = &nus_service_uuid.uuid;
    istick.discover_params.func = is_discover_cb;
    istick.discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    istick.discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    istick.discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(istick.conn, &istick.discover_params);
    if (err) {
        LOG_ERR("service discovery failed (err %d)", err);
        bt_conn_disconnect(istick.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

/* ── scanning ────────────────────────────────────────────────────────────── */

struct is_scan_ctx {
    bool is_inputstick;
};

static bool is_ad_parse_cb(struct bt_data *data, void *user_data) {
    struct is_scan_ctx *ctx = user_data;

    if (data->type != BT_DATA_NAME_COMPLETE && data->type != BT_DATA_NAME_SHORTENED) {
        return true;
    }

    /*
     * Match on the advertised NAME, not the service UUID: the dongle does not
     * put the NUS UUID in its advertisement, so a UUID filter finds nothing.
     * Same conclusion the browser prototype reached.
     */
    static const char prefix[] = "InputStick";
    const size_t prefix_len = sizeof(prefix) - 1;

    if (data->data_len >= prefix_len && memcmp(data->data, prefix, prefix_len) == 0) {
        ctx->is_inputstick = true;
        return false;
    }
    return true;
}

static void is_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf) {
    if (istick.state != IS_STATE_SCANNING) {
        return;
    }

    struct is_scan_ctx ctx = {.is_inputstick = false};
    struct net_buf_simple copy;
    net_buf_simple_clone(buf, &copy);
    bt_data_parse(&copy, is_ad_parse_cb, &ctx);

    if (!ctx.is_inputstick) {
        return;
    }

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));

    /*
     * A bound slot only ever talks to ITS dongle. Without this, selecting a
     * target would reach whichever dongle happened to answer first -- which
     * defeats the point of having per-machine targets, and would silently type
     * into the wrong computer.
     */
    if (slots[active_slot].bound) {
        if (bt_addr_le_cmp(&slots[active_slot].addr, info->addr) != 0) {
            LOG_DBG("ignoring %s, slot %u is bound elsewhere", addr_str, active_slot);
            return;
        }
        LOG_INF("found bound dongle for slot %u at %s (rssi %d)", active_slot, addr_str,
                info->rssi);
    } else {
        LOG_INF("binding slot %u to %s (rssi %d)", active_slot, addr_str, info->rssi);
        bt_addr_le_copy(&slots[active_slot].addr, info->addr);
        slots[active_slot].bound = true;
        is_slot_save(active_slot);
    }

    bt_addr_le_copy(&istick.addr, info->addr);
    istick.state = IS_STATE_CONNECTING;

    /* A connection cannot be created while the scanner is running. */
    int err = bt_le_scan_stop();
    if (err && err != -EALREADY) {
        LOG_WRN("scan stop returned %d (continuing)", err);
    }
    scan_running = false;

    err = bt_conn_le_create(info->addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT,
                            &istick.conn);
    if (err) {
        LOG_ERR("connect failed (err %d)", err);
        istick.state = IS_STATE_IDLE;
    }
}

static struct bt_le_scan_cb is_scan_cb = {
    .recv = is_scan_recv,
};

/* ── connection callbacks ────────────────────────────────────────────────── */

/* Only act on OUR connection -- hogp_central.c and the split link share these. */
static bool is_our_conn(struct bt_conn *conn) {
    return istick.conn != NULL && conn == istick.conn;
}

static void is_connected(struct bt_conn *conn, uint8_t err) {
    if (!is_our_conn(conn)) {
        return;
    }

    if (err) {
        LOG_ERR("connection failed (err %d)", err);
        bt_conn_unref(istick.conn);
        istick.conn = NULL;
        istick.state = IS_STATE_IDLE;
        return;
    }

    LOG_INF("connected to InputStick dongle");
    /*
     * No bt_conn_set_security() here: the dongle uses no pairing at all, so
     * asking for L2 only invites a needless SMP round trip (and a bond slot we
     * do not want to spend). Discovery goes straight ahead.
     */
    is_start_discovery();
}

static void is_disconnected(struct bt_conn *conn, uint8_t reason) {
    if (!is_our_conn(conn)) {
        return;
    }

    LOG_INF("InputStick dongle disconnected (reason 0x%02x)", reason);
    bt_conn_unref(istick.conn);
    istick.conn = NULL;
    istick.state = IS_STATE_IDLE;
    istick.write_handle = 0;
    istick.notify_handle = 0;
    istick.rx_len = 0;
    istick.fw_version = 0;
    istick.got_status = false;
}

static struct bt_conn_cb is_conn_callbacks = {
    .connected = is_connected,
    .disconnected = is_disconnected,
};

/* ── public API ──────────────────────────────────────────────────────────── */

void inputstick_start(void) {
    if (istick.state != IS_STATE_IDLE) {
        LOG_INF("already active (state %d)", istick.state);
        return;
    }

    istick.state = IS_STATE_SCANNING;

    int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL);
    if (err == -EALREADY) {
        /* HOGP pairing mode already has the scanner up; we share its reports. */
        LOG_INF("scanner already running (HOGP pairing?), reusing it");
        scan_running = true;
        return;
    }
    if (err) {
        LOG_ERR("scan start failed (err %d)", err);
        istick.state = IS_STATE_IDLE;
        return;
    }

    scan_running = true;
    LOG_INF("scanning for InputStick dongles...");
}

int inputstick_select_slot(uint8_t slot) {
    if (slot >= CONFIG_ZMK_INPUTSTICK_SLOT_COUNT) {
        LOG_ERR("slot %u out of range (max %d)", slot, CONFIG_ZMK_INPUTSTICK_SLOT_COUNT - 1);
        return -EINVAL;
    }

    if (slot != active_slot && istick.conn) {
        /* Different target: drop the current dongle before chasing the new one. */
        LOG_INF("switching slot %u -> %u, disconnecting current dongle", active_slot, slot);
        bt_conn_disconnect(istick.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    active_slot = slot;
    LOG_INF("slot %u selected (%s)", slot, slots[slot].bound ? "bound" : "OPEN - will bind");

    if (!inputstick_is_ready()) {
        inputstick_start();
    }
    return 0;
}

int inputstick_clear_slot(uint8_t slot) {
    if (slot >= CONFIG_ZMK_INPUTSTICK_SLOT_COUNT) {
        return -EINVAL;
    }

    LOG_INF("clearing slot %u", slot);
    slots[slot].bound = false;
    memset(&slots[slot].addr, 0, sizeof(slots[slot].addr));
    is_slot_save(slot);

    /* If we are connected on the slot being cleared, drop it so the next scan
     * can bind something new rather than silently keeping the old dongle. */
    if (slot == active_slot && istick.conn) {
        bt_conn_disconnect(istick.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    return 0;
}

void inputstick_stop(void) {
    if (scan_running) {
        bt_le_scan_stop();
        scan_running = false;
    }
    if (istick.conn) {
        bt_conn_disconnect(istick.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    istick.state = IS_STATE_IDLE;
    LOG_INF("InputStick stopped");
}

bool inputstick_is_ready(void) {
    return istick.state == IS_STATE_READY && istick.conn != NULL;
}

void inputstick_print_status(void) {
    static const char *names[] = {"IDLE", "SCANNING", "CONNECTING", "DISCOVERING", "HANDSHAKING",
                                  "READY"};

    /*
     * printk, not LOG_INF: status output must survive whatever the module log
     * level happens to be. !version was silently a no-op for exactly this
     * reason -- see the note in serial_cmd.c.
     */
    printk("=== InputStick ===\n");
    printk("State: %s\n", names[istick.state]);
    if (istick.conn) {
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(&istick.addr, addr_str, sizeof(addr_str));
        printk("Dongle: %s (%s)\n", addr_str, istick.legacy_hm ? "HM-10" : "NUS");
        printk("Handles: write=0x%04x notify=0x%04x\n", istick.write_handle, istick.notify_handle);
        printk("Firmware: %d  HID status seen: %s\n", istick.fw_version,
               istick.got_status ? "yes" : "no");
    } else {
        printk("Dongle: (not connected)\n");
    }
    printk("==================\n");
}

int inputstick_send_keys(uint8_t modifier, const uint8_t *keycodes, size_t count) {
    if (!inputstick_is_ready()) {
        return -ENOTCONN;
    }

    if (count > 6) {
        LOG_WRN("%zu keys held, truncating to 6 (no rollover-overflow report)", count);
        count = 6;
    }

    uint8_t report[8] = {modifier, 0x00};
    for (size_t i = 0; i < count; i++) {
        report[2 + i] = keycodes[i];
    }

    return is_send_packet(IS_CMD_HID_DATA_KEYB, 1, report, sizeof(report), false, NULL, NULL);
}

int inputstick_send_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
    if (!inputstick_is_ready()) {
        return -ENOTCONN;
    }

    uint8_t report[4] = {buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    return is_send_packet(IS_CMD_HID_DATA_MOUSE, 1, report, sizeof(report), false, NULL, NULL);
}

int inputstick_type(const char *text) {
    if (!inputstick_is_ready()) {
        LOG_WRN("not ready, cannot type");
        return -ENOTCONN;
    }

    for (const char *p = text; *p; p++) {
        uint8_t mod, key;
        if (!ascii_to_hid(*p, &mod, &key)) {
            LOG_WRN("no keycode for 0x%02x, skipping", (uint8_t)*p);
            continue;
        }

        /*
         * press-mod, press-mod+key, release-all -- batched into ONE packet
         * (param = report count) so the three reports cannot be split apart by
         * another sender interleaving between them.
         */
        uint8_t reports[24] = {0};
        reports[0] = mod;
        reports[8] = mod;
        reports[10] = key;
        /* reports[16..23] stay zero: all released */

        int err = is_send_packet(IS_CMD_HID_DATA_KEYB, 3, reports, sizeof(reports), false, NULL,
                                 NULL);
        if (err) {
            LOG_ERR("typing aborted at '%c' (err %d)", *p, err);
            return err;
        }
        k_msleep(CONFIG_ZMK_INPUTSTICK_TYPE_DELAY_MS);
    }

    return 0;
}

static int inputstick_init(void) {
    bt_conn_cb_register(&is_conn_callbacks);
    bt_le_scan_cb_register(&is_scan_cb);
    istick.state = IS_STATE_IDLE;
    LOG_INF("InputStick client initialised");
    return 0;
}

SYS_INIT(inputstick_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
