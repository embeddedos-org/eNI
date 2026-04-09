// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "generic.h"
#include "eni/log.h"
#include "eni/event.h"
#include <string.h>
#include <stdlib.h>

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
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

typedef struct {
    eni_transport_t transport;
    char            endpoint[256];
    uint32_t        timeout_ms;
    sock_t          sock;
    int             connected;
} generic_ctx_t;

/* Parse "host:port" from endpoint string */
static void parse_host_port(const char *endpoint, char *host, size_t host_size, int *port)
{
    *port = 9000; /* default port for generic BCI stream */
    const char *colon = strrchr(endpoint, ':');
    if (colon) {
        size_t hlen = (size_t)(colon - endpoint);
        if (hlen >= host_size) hlen = host_size - 1;
        memcpy(host, endpoint, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else {
        strncpy(host, endpoint, host_size - 1);
        host[host_size - 1] = '\0';
    }
}

static eni_status_t generic_init(eni_provider_t *prov, const void *config)
{
    generic_ctx_t *ctx = (generic_ctx_t *)calloc(1, sizeof(generic_ctx_t));
    if (!ctx) return ENI_ERR_NOMEM;

    ctx->sock = SOCK_INVALID;

    if (config) {
        const eni_generic_config_t *gcfg = (const eni_generic_config_t *)config;
        ctx->transport  = gcfg->transport;
        ctx->timeout_ms = gcfg->timeout_ms;
        if (gcfg->endpoint) {
            size_t len = strlen(gcfg->endpoint);
            if (len >= sizeof(ctx->endpoint)) len = sizeof(ctx->endpoint) - 1;
            memcpy(ctx->endpoint, gcfg->endpoint, len);
            ctx->endpoint[len] = '\0';
        }
    } else {
        ctx->transport  = ENI_TRANSPORT_TCP;
        ctx->timeout_ms = 1000;
    }

    prov->ctx = ctx;
    ENI_LOG_INFO("generic", "initialized (transport=%d endpoint=%s)",
                 ctx->transport, ctx->endpoint);
    return ENI_OK;
}

static eni_status_t generic_start(eni_provider_t *prov)
{
    generic_ctx_t *ctx = (generic_ctx_t *)prov->ctx;
    ENI_LOG_INFO("generic", "connecting to %s...", ctx->endpoint);

    if (strlen(ctx->endpoint) == 0) {
        ENI_LOG_WARN("generic", "no endpoint configured, operating in local mode");
        ctx->connected = 1;
        return ENI_OK;
    }

    char host[256] = {0};
    int port = 9000;
    parse_host_port(ctx->endpoint, host, sizeof(host), &port);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    if (ctx->transport == ENI_TRANSPORT_TCP) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);

        if (getaddrinfo(host, port_str, &hints, &res) != 0) {
            ENI_LOG_ERROR("generic", "DNS resolve failed for %s", host);
            return ENI_ERR_CONNECT;
        }

        ctx->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (ctx->sock == SOCK_INVALID) {
            freeaddrinfo(res);
            return ENI_ERR_CONNECT;
        }

        if (connect(ctx->sock, res->ai_addr, (socklen_t)res->ai_addrlen) != 0) {
            ENI_LOG_ERROR("generic", "TCP connect failed to %s:%d", host, port);
            sock_close(ctx->sock);
            ctx->sock = SOCK_INVALID;
            freeaddrinfo(res);
            return ENI_ERR_CONNECT;
        }
        freeaddrinfo(res);

        ENI_LOG_INFO("generic", "TCP connected to %s:%d", host, port);

    } else if (ctx->transport == ENI_TRANSPORT_SHARED_MEM) {
        /* Treat shared memory transport like UDP for now */
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);

        if (getaddrinfo(host, port_str, &hints, &res) != 0) {
            ENI_LOG_ERROR("generic", "DNS resolve failed for %s", host);
            return ENI_ERR_CONNECT;
        }

        ctx->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (ctx->sock == SOCK_INVALID) {
            freeaddrinfo(res);
            return ENI_ERR_CONNECT;
        }

        /* For UDP, connect() sets the default destination */
        if (connect(ctx->sock, res->ai_addr, (socklen_t)res->ai_addrlen) != 0) {
            ENI_LOG_ERROR("generic", "UDP connect failed to %s:%d", host, port);
            sock_close(ctx->sock);
            ctx->sock = SOCK_INVALID;
            freeaddrinfo(res);
            return ENI_ERR_CONNECT;
        }
        freeaddrinfo(res);

        ENI_LOG_INFO("generic", "UDP bound to %s:%d", host, port);
    } else {
        ENI_LOG_WARN("generic", "transport %d not yet supported, using local mode",
                     ctx->transport);
    }

    ctx->connected = 1;
    return ENI_OK;
}

static eni_status_t generic_poll(eni_provider_t *prov, eni_event_t *ev)
{
    generic_ctx_t *ctx = (generic_ctx_t *)prov->ctx;
    if (!ctx->connected || ctx->sock == SOCK_INVALID) {
        return ENI_ERR_TIMEOUT;
    }

    /* Poll socket for incoming data with timeout */
#ifndef _WIN32
    struct pollfd pfd;
    pfd.fd = ctx->sock;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, (int)ctx->timeout_ms);
    if (ret <= 0) return ENI_ERR_TIMEOUT;

    /* Read incoming data */
    uint8_t buf[4096];
    ssize_t n = recv(ctx->sock, (char *)buf, sizeof(buf), 0);
    if (n <= 0) return ENI_ERR_TIMEOUT;

    /* Parse received data into event */
    if (ev) {
        ev->type = ENI_EVENT_RAW;
        size_t copy_len = (size_t)n < ENI_EVENT_PAYLOAD_MAX ? (size_t)n : ENI_EVENT_PAYLOAD_MAX;
        memcpy(ev->payload.raw.data, buf, copy_len);
        ev->payload.raw.len = copy_len;
    }
    return ENI_OK;
#else
    /* Windows: use select() */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ctx->sock, &readfds);

    struct timeval tv;
    tv.tv_sec = (long)(ctx->timeout_ms / 1000);
    tv.tv_usec = (long)((ctx->timeout_ms % 1000) * 1000);

    int ret = select(0, &readfds, NULL, NULL, &tv);
    if (ret <= 0) return ENI_ERR_TIMEOUT;

    uint8_t buf[4096];
    int n = recv(ctx->sock, (char *)buf, sizeof(buf), 0);
    if (n <= 0) return ENI_ERR_TIMEOUT;

    if (ev) {
        ev->type = ENI_EVENT_RAW;
        size_t copy_len = (size_t)n < ENI_EVENT_PAYLOAD_MAX ? (size_t)n : ENI_EVENT_PAYLOAD_MAX;
        memcpy(ev->payload.raw.data, buf, copy_len);
        ev->payload.raw.len = copy_len;
    }
    return ENI_OK;
#endif
}

static eni_status_t generic_stop(eni_provider_t *prov)
{
    generic_ctx_t *ctx = (generic_ctx_t *)prov->ctx;
    if (ctx->sock != SOCK_INVALID) {
        sock_close(ctx->sock);
        ctx->sock = SOCK_INVALID;
    }
    ctx->connected = 0;
    ENI_LOG_INFO("generic", "disconnected");
    return ENI_OK;
}

static void generic_shutdown(eni_provider_t *prov)
{
    if (prov && prov->ctx) {
        generic_ctx_t *ctx = (generic_ctx_t *)prov->ctx;
        if (ctx->sock != SOCK_INVALID) {
            sock_close(ctx->sock);
        }
        free(prov->ctx);
        prov->ctx = NULL;
    }
    ENI_LOG_INFO("generic", "shutdown");
}

const eni_provider_ops_t eni_provider_generic_ops = {
    .name     = "generic",
    .init     = generic_init,
    .poll     = generic_poll,
    .start    = generic_start,
    .stop     = generic_stop,
    .shutdown = generic_shutdown,
};
