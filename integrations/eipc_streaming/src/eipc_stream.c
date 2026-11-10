// SPDX-License-Identifier: MIT
// eNI EIPC Streaming — Real-time neural data streaming implementation

#include "eni/eipc_stream.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

/* EIPC stream packet header for wire format */
#define EIPC_STREAM_MAGIC  0x454E4953 /* "ENIS" */
#define EIPC_STREAM_VERSION 1

typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  compression;
    uint16_t reserved;
    uint32_t sequence;
    uint64_t timestamp;
    uint32_t num_samples;
    uint32_t data_len;
} __attribute__((packed)) eipc_wire_header_t;

/* Session-internal socket handle */
static sock_t g_stream_sock = SOCK_INVALID;

eni_stream_status_t eni_stream_init(eni_stream_session_t *session, const eni_stream_config_t *config)
{
    if (!session || !config) return ENI_STREAM_ERR_INIT;
    memset(session, 0, sizeof(*session));
    session->config = *config;

    if (session->config.buffer_size == 0) session->config.buffer_size = ENI_STREAM_MAX_PACKET_SIZE;
    if (session->config.max_backpressure == 0) session->config.max_backpressure = ENI_STREAM_RING_CAPACITY / 2;

    session->initialized = 1;
    return ENI_STREAM_OK;
}

eni_stream_status_t eni_stream_connect(eni_stream_session_t *session)
{
    if (!session || !session->initialized) return ENI_STREAM_ERR_INIT;

    const char *endpoint = session->config.endpoint;
    uint16_t port = session->config.port;

    if (!endpoint || port == 0) {
        /* No endpoint configured — operate in local buffer mode */
        session->connected = 1;
        return ENI_STREAM_OK;
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);

    if (getaddrinfo(endpoint, port_str, &hints, &res) != 0) {
        return ENI_STREAM_ERR_CONNECT;
    }

    g_stream_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (g_stream_sock == SOCK_INVALID) {
        freeaddrinfo(res);
        return ENI_STREAM_ERR_CONNECT;
    }

    /* Set TCP_NODELAY for low-latency streaming */
    int flag = 1;
    setsockopt(g_stream_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(flag));

    if (connect(g_stream_sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        sock_close(g_stream_sock);
        g_stream_sock = SOCK_INVALID;
        freeaddrinfo(res);
        return ENI_STREAM_ERR_CONNECT;
    }
    freeaddrinfo(res);

    session->connected = 1;
    return ENI_STREAM_OK;
}

static eni_stream_status_t delta_compress(const float *samples, uint32_t count, uint8_t *out, uint32_t *out_len, uint32_t max_len)
{
    if (count * sizeof(float) > max_len) return ENI_STREAM_ERR_COMPRESS;

    /* Simple delta encoding: store first sample as-is, then deltas */
    if (count == 0) { *out_len = 0; return ENI_STREAM_OK; }

    memcpy(out, &samples[0], sizeof(float));
    uint32_t pos = sizeof(float);

    for (uint32_t i = 1; i < count && pos + sizeof(float) <= max_len; i++) {
        float delta = samples[i] - samples[i - 1];
        memcpy(out + pos, &delta, sizeof(float));
        pos += sizeof(float);
    }
    *out_len = pos;
    return ENI_STREAM_OK;
}

/* Send data over the wire with EIPC header */
static eni_stream_status_t stream_send_packet(eni_stream_session_t *session,
                                                const eni_stream_packet_t *pkt)
{
    if (g_stream_sock == SOCK_INVALID) {
        /* No socket — local buffer mode, just count */
        return ENI_STREAM_OK;
    }

    /* Build wire header */
    eipc_wire_header_t hdr;
    hdr.magic = EIPC_STREAM_MAGIC;
    hdr.version = EIPC_STREAM_VERSION;
    hdr.compression = (uint8_t)session->config.compression;
    hdr.reserved = 0;
    hdr.sequence = pkt->sequence;
    hdr.timestamp = pkt->timestamp;
    hdr.num_samples = pkt->num_samples;
    hdr.data_len = pkt->data_len;

    /* Send header */
    size_t sent = 0;
    const uint8_t *buf = (const uint8_t *)&hdr;
    size_t total = sizeof(hdr);
    while (sent < total) {
        int n = send(g_stream_sock, (const char *)(buf + sent), (int)(total - sent), 0);
        if (n <= 0) return ENI_STREAM_ERR_SEND;
        sent += (size_t)n;
    }

    /* Send data */
    sent = 0;
    while (sent < pkt->data_len) {
        int n = send(g_stream_sock, (const char *)(pkt->data + sent), (int)(pkt->data_len - sent), 0);
        if (n <= 0) return ENI_STREAM_ERR_SEND;
        sent += (size_t)n;
    }

    return ENI_STREAM_OK;
}

eni_stream_status_t eni_stream_send(eni_stream_session_t *session, const float *samples, uint32_t count, uint64_t timestamp)
{
    if (!session || !session->initialized || !samples) return ENI_STREAM_ERR_INIT;
    if (!session->connected) return ENI_STREAM_ERR_CONNECT;

    if ((uint32_t)session->ring_count >= session->config.max_backpressure) {
        session->drops++;
        return ENI_STREAM_ERR_BACKPRESSURE;
    }

    eni_stream_packet_t *pkt = &session->ring[session->ring_head];
    pkt->timestamp = timestamp;
    pkt->sequence = session->sequence++;
    pkt->num_samples = count;

    if (session->config.compression == ENI_STREAM_COMPRESS_DELTA) {
        eni_stream_status_t st = delta_compress(samples, count, pkt->data, &pkt->data_len, ENI_STREAM_MAX_PACKET_SIZE);
        if (st != ENI_STREAM_OK) return st;
    } else {
        uint32_t bytes = count * sizeof(float);
        if (bytes > ENI_STREAM_MAX_PACKET_SIZE) bytes = ENI_STREAM_MAX_PACKET_SIZE;
        memcpy(pkt->data, samples, bytes);
        pkt->data_len = bytes;
    }

    session->ring_head = (session->ring_head + 1) % ENI_STREAM_RING_CAPACITY;
    session->ring_count++;

    /* Send the packet over the wire */
    eni_stream_status_t send_st = stream_send_packet(session, pkt);
    if (send_st != ENI_STREAM_OK) {
        /* On send failure, keep packet in ring for retry */
        if (session->config.auto_reconnect) {
            /* Try to reconnect */
            sock_close(g_stream_sock);
            g_stream_sock = SOCK_INVALID;
            session->connected = 0;
            eni_stream_connect(session);
        }
        return send_st;
    }

    session->bytes_sent += pkt->data_len;
    session->packets_sent++;
    session->ring_count--;
    session->ring_tail = (session->ring_tail + 1) % ENI_STREAM_RING_CAPACITY;

    return ENI_STREAM_OK;
}

eni_stream_status_t eni_stream_flush(eni_stream_session_t *session)
{
    if (!session || !session->initialized) return ENI_STREAM_ERR_INIT;

    /* Flush remaining packets in the ring buffer */
    while (session->ring_count > 0) {
        eni_stream_packet_t *pkt = &session->ring[session->ring_tail];
        eni_stream_status_t st = stream_send_packet(session, pkt);
        if (st != ENI_STREAM_OK) {
            /* Can't flush, discard remaining */
            break;
        }
        session->bytes_sent += pkt->data_len;
        session->packets_sent++;
        session->ring_tail = (session->ring_tail + 1) % ENI_STREAM_RING_CAPACITY;
        session->ring_count--;
    }

    /* Discard any remaining unsent packets */
    session->ring_head = 0;
    session->ring_tail = 0;
    session->ring_count = 0;

    return ENI_STREAM_OK;
}

eni_stream_status_t eni_stream_disconnect(eni_stream_session_t *session)
{
    if (!session || !session->initialized) return ENI_STREAM_ERR_INIT;
    eni_stream_flush(session);

    if (g_stream_sock != SOCK_INVALID) {
        sock_close(g_stream_sock);
        g_stream_sock = SOCK_INVALID;
    }

    session->connected = 0;
    return ENI_STREAM_OK;
}

void eni_stream_get_stats(const eni_stream_session_t *session, uint64_t *bytes, uint32_t *packets, uint32_t *drops)
{
    if (!session) return;
    if (bytes) *bytes = session->bytes_sent;
    if (packets) *packets = session->packets_sent;
    if (drops) *drops = session->drops;
}

void eni_stream_deinit(eni_stream_session_t *session)
{
    if (session) {
        if (session->connected) {
            eni_stream_disconnect(session);
        }
        memset(session, 0, sizeof(*session));
    }
}
