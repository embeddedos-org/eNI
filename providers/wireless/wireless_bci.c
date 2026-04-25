/**
 * @file wireless_bci.c
 * @brief Wireless BCI provider implementation.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wireless_bci.h"
#include <string.h>
#include <math.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

/* BlueZ HCI headers */
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#endif /* __linux__ */

/* ---- helpers ------------------------------------------------------------ */

static inline uint32_t ring_next(uint32_t idx, uint32_t size)
{
    return (idx + 1) % size;
}

static void decode_raw_to_samples(const eni_wbci_raw_pkt_t *pkt,
                                  float *samples,
                                  uint32_t num_channels)
{
    /*
     * Protocol: each channel encoded as 3-byte signed 24-bit big-endian.
     * Header: 1 byte seq + 1 byte status = 2 bytes offset.
     */
    uint32_t offset = 2;
    for (uint32_t ch = 0; ch < num_channels; ch++) {
        if (offset + 3 > pkt->len) {
            samples[ch] = 0.0f;
            continue;
        }
        int32_t raw = ((int32_t)pkt->raw[offset] << 16) |
                      ((int32_t)pkt->raw[offset + 1] << 8) |
                      ((int32_t)pkt->raw[offset + 2]);
        /* Sign extension from 24-bit */
        if (raw & 0x800000) {
            raw |= (int32_t)0xFF000000;
        }
        /* Convert to uV: +/-187.5 uV full-scale for 24-bit ADC */
        samples[ch] = (float)raw * (187.5f / 8388607.0f);
        offset += 3;
    }
}

static uint8_t compute_checksum(const uint8_t *data, uint32_t len)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum;
}

/* ---- Provider vtable wrappers ------------------------------------------- */

static int wbci_vtable_init(void *ctx, const void *config)
{
    return eni_wbci_init((eni_wbci_t *)ctx, (const eni_wbci_config_t *)config);
}

static int wbci_vtable_connect(void *ctx)
{
    return eni_wbci_connect((eni_wbci_t *)ctx);
}

static int wbci_vtable_poll(void *ctx, float *samples,
                            uint32_t *num_channels, uint64_t *timestamp)
{
    return eni_wbci_poll((eni_wbci_t *)ctx, samples, num_channels, timestamp);
}

static int wbci_vtable_disconnect(void *ctx)
{
    return eni_wbci_disconnect((eni_wbci_t *)ctx);
}

static int wbci_vtable_deinit(void *ctx)
{
    return eni_wbci_deinit((eni_wbci_t *)ctx);
}

static const eni_wbci_ops_t wbci_ops = {
    .init       = wbci_vtable_init,
    .connect    = wbci_vtable_connect,
    .poll       = wbci_vtable_poll,
    .disconnect = wbci_vtable_disconnect,
    .deinit     = wbci_vtable_deinit,
    .name       = "wireless_bci"
};

/* ---- Public API --------------------------------------------------------- */

int eni_wbci_init(eni_wbci_t *bci, const eni_wbci_config_t *config)
{
    if (!bci || !config) return ENI_WBCI_ERR_NULL;
    if (config->num_channels == 0 ||
        config->num_channels > ENI_WBCI_MAX_CHANNELS) return ENI_WBCI_ERR_PARAM;

    memset(bci, 0, sizeof(*bci));
    memcpy(&bci->config, config, sizeof(*config));

    if (bci->config.sample_rate_hz == 0) {
        bci->config.sample_rate_hz = ENI_WBCI_SAMPLE_RATE_HZ;
    }
    if (bci->config.connect_timeout_ms == 0) {
        bci->config.connect_timeout_ms = 5000;
    }
    if (bci->config.poll_interval_ms == 0) {
        bci->config.poll_interval_ms = 4; /* 250 Hz */
    }

    bci->state = ENI_WBCI_STATE_IDLE;
    bci->initialised = true;
    return ENI_WBCI_OK;
}

int eni_wbci_connect(eni_wbci_t *bci)
{
    if (!bci) return ENI_WBCI_ERR_NULL;
    if (!bci->initialised) return ENI_WBCI_ERR_INIT;

    if (bci->state == ENI_WBCI_STATE_CONNECTED ||
        bci->state == ENI_WBCI_STATE_STREAMING) {
        return ENI_WBCI_ERR_BUSY;
    }

    bci->state = ENI_WBCI_STATE_SCANNING;

#ifdef __linux__
    /*
     * BLE scanning and connection using Linux BlueZ HCI socket API.
     *
     * 1. Open HCI socket and get device ID
     * 2. Set LE scan parameters and enable scanning
     * 3. Read scan results, match against target device_addr
     * 4. Disable scanning
     * 5. Establish L2CAP BLE connection to matched device
     */

    int hci_dev_id = hci_get_route(NULL);
    if (hci_dev_id < 0) {
        bci->state = ENI_WBCI_STATE_ERROR;
        return ENI_WBCI_ERR_IO;
    }

    int hci_sock = hci_open_dev(hci_dev_id);
    if (hci_sock < 0) {
        bci->state = ENI_WBCI_STATE_ERROR;
        return ENI_WBCI_ERR_IO;
    }

    /* Set LE scan parameters: active scan, 10ms interval, 10ms window */
    uint8_t scan_type = 0x01;        /* active scan */
    uint16_t interval = htobs(0x0010); /* 10ms */
    uint16_t window = htobs(0x0010);   /* 10ms */
    uint8_t own_type = LE_PUBLIC_ADDRESS;
    uint8_t filter_policy = 0x00;    /* accept all */

    int ret = hci_le_set_scan_parameters(
        hci_sock, scan_type, interval, window,
        own_type, filter_policy, 1000  /* 1s timeout */
    );
    if (ret < 0) {
        close(hci_sock);
        bci->state = ENI_WBCI_STATE_ERROR;
        return ENI_WBCI_ERR_IO;
    }

    /* Enable LE scanning */
    ret = hci_le_set_scan_enable(hci_sock, 0x01, 0x00, 1000);
    if (ret < 0) {
        close(hci_sock);
        bci->state = ENI_WBCI_STATE_ERROR;
        return ENI_WBCI_ERR_IO;
    }

    /*
     * Read HCI events to find our target device.
     * We look for LE advertising reports matching device_addr.
     */
    bdaddr_t target_addr;
    memcpy(&target_addr, bci->config.device_addr, 6);

    bool found = false;
    uint32_t timeout_ms = bci->config.connect_timeout_ms;
    uint32_t elapsed = 0;

    struct hci_filter old_filt, new_filt;
    socklen_t filt_len = sizeof(old_filt);
    getsockopt(hci_sock, SOL_HCI, HCI_FILTER, &old_filt, &filt_len);

    hci_filter_clear(&new_filt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &new_filt);
    hci_filter_set_event(EVT_LE_META_EVENT, &new_filt);
    setsockopt(hci_sock, SOL_HCI, HCI_FILTER, &new_filt, sizeof(new_filt));

    while (!found && elapsed < timeout_ms) {
        uint8_t buf[HCI_MAX_EVENT_SIZE];
        ssize_t n = read(hci_sock, buf, sizeof(buf));
        if (n <= 0) {
            usleep(10000); /* 10ms */
            elapsed += 10;
            continue;
        }

        evt_le_meta_event *meta = (evt_le_meta_event *)(buf + HCI_EVENT_HDR_SIZE + 1);
        if (meta->subevent != EVT_LE_ADVERTISING_REPORT)
            continue;

        le_advertising_info *adv = (le_advertising_info *)(meta->data + 1);

        /* Compare with target address */
        if (bacmp(&adv->bdaddr, &target_addr) == 0) {
            found = true;
            bci->rssi = (uint8_t)((int8_t)buf[n - 1] + 128);
        }
    }

    /* Disable scanning */
    hci_le_set_scan_enable(hci_sock, 0x00, 0x00, 1000);
    setsockopt(hci_sock, SOL_HCI, HCI_FILTER, &old_filt, sizeof(old_filt));
    close(hci_sock);

    if (!found) {
        bci->state = ENI_WBCI_STATE_IDLE;
        return ENI_WBCI_ERR_NOT_FOUND;
    }

    /*
     * Establish L2CAP BLE connection.
     * BLE uses L2CAP with CID 0x0004 (ATT) on LE transport.
     */
    bci->state = ENI_WBCI_STATE_CONNECTING;

    int l2cap_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (l2cap_sock < 0) {
        bci->state = ENI_WBCI_STATE_ERROR;
        return ENI_WBCI_ERR_IO;
    }

    /* Bind to local adapter */
    struct sockaddr_l2 local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.l2_family = AF_BLUETOOTH;
    local_addr.l2_bdaddr = *BDADDR_ANY;
    local_addr.l2_cid = htobs(4); /* ATT CID */
    local_addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;

    if (bind(l2cap_sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        close(l2cap_sock);
        bci->state = ENI_WBCI_STATE_ERROR;
        return ENI_WBCI_ERR_IO;
    }

    /* Connect to remote device */
    struct sockaddr_l2 remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.l2_family = AF_BLUETOOTH;
    memcpy(&remote_addr.l2_bdaddr, bci->config.device_addr, 6);
    remote_addr.l2_cid = htobs(4);
    remote_addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;

    /* Set connection timeout */
    struct timeval tv;
    tv.tv_sec = bci->config.connect_timeout_ms / 1000;
    tv.tv_usec = (bci->config.connect_timeout_ms % 1000) * 1000;
    setsockopt(l2cap_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(l2cap_sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        close(l2cap_sock);
        if (errno == ETIMEDOUT) {
            bci->state = ENI_WBCI_STATE_IDLE;
            return ENI_WBCI_ERR_TIMEOUT;
        }
        bci->state = ENI_WBCI_STATE_ERROR;
        return ENI_WBCI_ERR_IO;
    }

    /* Connection established — store the socket fd for later use.
     * In a production implementation, we'd store this in an extended context. */
    close(l2cap_sock);  /* Close for now; data streaming handled separately */

    bci->state = ENI_WBCI_STATE_CONNECTED;
    bci->link_quality = 95;
    return ENI_WBCI_OK;

#else
    /*
     * Non-Linux platform: simulate connection (stub for portability).
     * Transition directly to connected state.
     */
    bci->state = ENI_WBCI_STATE_CONNECTED;
    bci->rssi = 80;
    bci->link_quality = 95;
    return ENI_WBCI_OK;
#endif /* __linux__ */
}

int eni_wbci_poll(eni_wbci_t *bci, float *samples,
                  uint32_t *num_channels, uint64_t *timestamp)
{
    if (!bci || !samples) return ENI_WBCI_ERR_NULL;
    if (!bci->initialised) return ENI_WBCI_ERR_INIT;

    if (bci->state != ENI_WBCI_STATE_CONNECTED &&
        bci->state != ENI_WBCI_STATE_STREAMING) {
        return ENI_WBCI_ERR_INIT;
    }

    bci->state = ENI_WBCI_STATE_STREAMING;

    /* Check receive ring buffer */
    if (bci->rx_count > 0) {
        eni_wbci_raw_pkt_t *pkt = &bci->rx_ring[bci->rx_tail];

        /* Verify checksum */
        if (pkt->len > 1) {
            uint8_t expected = compute_checksum(pkt->raw, pkt->len - 1);
            if (pkt->raw[pkt->len - 1] != expected) {
                bci->crc_errors++;
                bci->rx_tail = ring_next(bci->rx_tail, ENI_WBCI_RX_RING_SIZE);
                bci->rx_count--;
                return ENI_WBCI_ERR_IO;
            }
        }

        decode_raw_to_samples(pkt, bci->samples, bci->config.num_channels);
        bci->sample_seq++;
        bci->pkts_received++;

        bci->rx_tail = ring_next(bci->rx_tail, ENI_WBCI_RX_RING_SIZE);
        bci->rx_count--;

        memcpy(samples, bci->samples,
               bci->config.num_channels * sizeof(float));
        if (num_channels) *num_channels = bci->config.num_channels;
        if (timestamp)    *timestamp = pkt->timestamp;

        return ENI_WBCI_OK;
    }

    /* No data available -- return last known samples */
    memcpy(samples, bci->samples,
           bci->config.num_channels * sizeof(float));
    if (num_channels) *num_channels = bci->config.num_channels;
    if (timestamp)    *timestamp = 0;

    return ENI_WBCI_ERR_TIMEOUT;
}

int eni_wbci_disconnect(eni_wbci_t *bci)
{
    if (!bci) return ENI_WBCI_ERR_NULL;
    if (!bci->initialised) return ENI_WBCI_ERR_INIT;

    bci->state = ENI_WBCI_STATE_IDLE;
    bci->rx_head  = 0;
    bci->rx_tail  = 0;
    bci->rx_count = 0;

    return ENI_WBCI_OK;
}

int eni_wbci_deinit(eni_wbci_t *bci)
{
    if (!bci) return ENI_WBCI_ERR_NULL;

    if (bci->state == ENI_WBCI_STATE_CONNECTED ||
        bci->state == ENI_WBCI_STATE_STREAMING) {
        eni_wbci_disconnect(bci);
    }

    memset(bci, 0, sizeof(*bci));
    return ENI_WBCI_OK;
}

const eni_wbci_ops_t *eni_wbci_get_ops(void)
{
    return &wbci_ops;
}
